/****************************************************************************
 * arch/arm/src/am67/am67_tisci.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ask the DMSC, over the secure proxy, to turn a device on. EPWM0/1 stay
 * powered down until somebody sends TISCI_MSG_SET_DEVICE; a powered-off
 * module reads as zero and the PWM setup bails out before pinmux.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/mutex.h>

#include "arm_internal.h"
#include "am67_tisci.h"

/* tisci/tisci.h uses begin_packed, which this NuttX does not define, so the
 * few constants this file needs are repeated here. */

#define TISCI_MSG_SYS_RESET                      0x0005u
#define TISCI_MSG_SET_DEVICE                     0x0200u
#define TISCI_MSG_GET_FREQ                       0x010eu
#define TISCI_MSG_GET_DEVICE                     0x0201u
#define TISCI_MSG_FLAG_AOP                       (1u << 1)
#define TISCI_MSG_FLAG_ACK                       (1u << 1)
#define TISCI_MSG_VALUE_DEVICE_SW_STATE_ON       2u
#define TISCI_HOST_ID_MAIN_0_R5_1                38u

#define SEC_PROXY_TARGET_BASE                    0x4d000000u
#define SEC_PROXY_RT_BASE                        0x4a600000u
#define SEC_PROXY_THREAD_SIZE                    0x00001000u
/* J722S TISCI sec_proxy.html: non-secure MAIN R5 context is host
 * MAIN_0_R5_1. Thread 6 is its response (read), thread 7 its
 * low_priority (write). Threads 4/5 belong to the secure context. */
#define MAIN_R5_NS_RESPONSE_THRD_ID              6u
#define MAIN_R5_NS_WRITE_THRD_ID                 7u
#define MAIN_R5_NS_TX_ADDR                       (SEC_PROXY_TARGET_BASE + \
                                                  (MAIN_R5_NS_WRITE_THRD_ID * \
                                                   SEC_PROXY_THREAD_SIZE))
#define MAIN_R5_NS_RX_ADDR                       (SEC_PROXY_TARGET_BASE + \
                                                  (MAIN_R5_NS_RESPONSE_THRD_ID * \
                                                   SEC_PROXY_THREAD_SIZE))

#define SPROXY_DATA_FIRST  0x4u
#define SPROXY_DATA_LAST   0x3cu
#define SPROXY_COUNT_MASK  0xffu

/* Longest wait for the Device Manager's response. */

#define SPROXY_TIMEOUT_US  10000u

/* One request in flight at a time: pwm_out, the I2C work queue and board
 * init can all power a device, and they share one secure proxy thread.
 */

static mutex_t g_lock = NXMUTEX_INITIALIZER;
static uint8_t g_seq;

static void sproxy_read_msg(uint32_t base)
{
  uint32_t off;

  for (off = SPROXY_DATA_FIRST; off <= SPROXY_DATA_LAST; off += 4u)
    {
      (void)getreg32(base + off);
    }
}

/* Send one message and read the response whose header (type, host, seq)
 * matches it.  A response left over from an earlier request that timed
 * out is read and dropped, instead of being taken for this one.
 */

static int sproxy_xfer(const void *tx, size_t txlen,
                       void *rx, size_t rxlen)
{
  const uint32_t tx_base = MAIN_R5_NS_TX_ADDR;
  const uint32_t rx_base = MAIN_R5_NS_RX_ADDR;
  const uint32_t tx_rt = SEC_PROXY_RT_BASE +
                         (MAIN_R5_NS_WRITE_THRD_ID * SEC_PROXY_THREAD_SIZE);
  const uint32_t rx_rt = SEC_PROXY_RT_BASE +
                         (MAIN_R5_NS_RESPONSE_THRD_ID * SEC_PROXY_THREAD_SIZE);
  const uint8_t *src = tx;
  uint8_t *dst = rx;
  uint32_t off;
  size_t left = txlen;
  size_t got = 0;
  unsigned int spins;
  uint32_t hdr;

  memcpy(&hdr, tx, sizeof(hdr));

  if ((getreg32(tx_rt) & SPROXY_COUNT_MASK) == 0u)
    {
      return -EBUSY;
    }

  if ((getreg32(rx_rt) & SPROXY_COUNT_MASK) != 0u)
    {
      sproxy_read_msg(rx_base);
    }

  for (off = SPROXY_DATA_FIRST; off <= SPROXY_DATA_LAST; off += 4u)
    {
      uint32_t word = 0;

      if (left > 0u)
        {
          size_t n = left > 4u ? 4u : left;

          memcpy(&word, src, n);
          src += n;
          left -= n;
        }

      putreg32(word, tx_base + off);
    }

  UP_DSB();

  for (spins = 0; ; spins++)
    {
      if ((getreg32(rx_rt) & SPROXY_COUNT_MASK) != 0u)
        {
          /* The first word is type (16), host (8), seq (8). */

          if (getreg32(rx_base + SPROXY_DATA_FIRST) == hdr)
            {
              break;
            }

          sproxy_read_msg(rx_base);
          continue;
        }

      if (spins >= SPROXY_TIMEOUT_US)
        {
          return -ETIMEDOUT;
        }

      up_udelay(1);
    }

  for (off = SPROXY_DATA_FIRST; off <= SPROXY_DATA_LAST; off += 4u)
    {
      uint32_t word = getreg32(rx_base + off);

      if (dst != NULL && got < rxlen)
        {
          size_t n = rxlen - got;

          if (n > 4u)
            {
              n = 4u;
            }

          memcpy(dst + got, &word, n);
          got += n;
        }
    }

  return 0;
}

int am67_tisci_device_on(uint32_t id)
{
  uint8_t raw[17];
  struct
  {
    uint16_t type;
    uint8_t host;
    uint8_t seq;
    uint32_t flags;
    uint32_t id;
    uint32_t reserved;
    uint8_t state;
  } req;
  struct
  {
    uint16_t type;
    uint8_t host;
    uint8_t seq;
    uint32_t flags;
  } resp;
  int ret;

  memset(&req, 0, sizeof(req));
  memset(&resp, 0, sizeof(resp));

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  req.type = TISCI_MSG_SET_DEVICE;
  req.host = TISCI_HOST_ID_MAIN_0_R5_1;
  req.seq = g_seq++;
  req.flags = TISCI_MSG_FLAG_AOP;
  req.id = id;
  req.state = TISCI_MSG_VALUE_DEVICE_SW_STATE_ON;

  /* The wire format is packed. The local struct may insert padding. */
  memcpy(raw + 0, &req.type, 2);
  raw[2] = req.host;
  raw[3] = req.seq;
  memcpy(raw + 4, &req.flags, 4);
  memcpy(raw + 8, &req.id, 4);
  memcpy(raw + 12, &req.reserved, 4);
  raw[16] = req.state;

  ret = sproxy_xfer(raw, sizeof(raw), &resp, sizeof(resp));
  nxmutex_unlock(&g_lock);

  if (ret < 0)
    {
      syslog(LOG_ERR, "tisci dev %" PRIu32 " on: xfer %d\n", id, ret);
      return ret;
    }

  if ((resp.flags & TISCI_MSG_FLAG_ACK) == 0u)
    {
      syslog(LOG_ERR, "tisci dev %" PRIu32 " on: nak flags 0x%" PRIx32 "\n",
             id, resp.flags);
      return -EIO;
    }

  syslog(LOG_INFO, "tisci dev %" PRIu32 " on: ack\n", id);
  return 0;
}

/* Ask the DM to reset the whole SoC (as Linux reboot does): the R5F needs
 * nobody else to restart.  am67_tisci_sys_reset_now() takes no lock and
 * logs nothing, for a crash or watchdog handler with interrupts off: a
 * request another context left half-written in the proxy thread can only
 * make the DM refuse this one.  Both return only if the DM refused (-EIO)
 * or did not answer; on success the SoC, this core included, resets.
 */

static int sys_reset_xfer(uint8_t seq)
{
  uint16_t type = TISCI_MSG_SYS_RESET;
  uint32_t flags = TISCI_MSG_FLAG_AOP;
  uint8_t req[8];
  uint8_t resp[8];
  int ret;

  memset(req, 0, sizeof(req));
  memset(resp, 0, sizeof(resp));
  memcpy(req + 0, &type, 2);
  req[2] = TISCI_HOST_ID_MAIN_0_R5_1;
  req[3] = seq;
  memcpy(req + 4, &flags, 4);

  ret = sproxy_xfer(req, sizeof(req), resp, sizeof(resp));
  if (ret < 0)
    {
      return ret;
    }

  memcpy(&flags, resp + 4, 4);
  return (flags & TISCI_MSG_FLAG_ACK) ? 0 : -EIO;
}

int am67_tisci_sys_reset_now(void)
{
  return sys_reset_xfer(g_seq++);
}

int am67_tisci_sys_reset(void)
{
  int ret;

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  ret = sys_reset_xfer(g_seq++);
  nxmutex_unlock(&g_lock);

  syslog(LOG_ERR, "tisci sys reset refused or unanswered: %d\n", ret);
  return ret;
}

/* Make sure device `id` stays powered while this core uses it.  A request
 * of our own makes the DM keep the device on whatever the other hosts do
 * (Linux or U-Boot releasing it, a runtime-PM suspend).  A device another
 * host holds exclusively answers NAK; it then works only for as long as
 * that host keeps it on, which GET_DEVICE confirms for now.
 *
 * Returns 0 when this core holds the device, 1 when it is on under another
 * host's request only, a negated errno when it is off or the DM does not
 * answer (touching it would then abort).
 */

int am67_tisci_device_require(uint32_t id)
{
  uint8_t programmed = 0;
  uint8_t current = 0;
  int ret;

  ret = am67_tisci_device_on(id);
  if (ret == 0)
    {
      return 0;
    }

  if (am67_tisci_get_device(id, &programmed, &current) < 0)
    {
      return ret;
    }

  if (current == 1)
    {
      syslog(LOG_WARNING, "tisci dev %" PRIu32 ": on, held by another "
             "host\n", id);
      return 1;
    }

  syslog(LOG_ERR, "tisci dev %" PRIu32 ": off (programmed %u current %u)\n",
         id, programmed, current);
  return -ENODEV;
}

/* Read the current frequency of clock `clk` of device `dev` (the Linux
 * DTS <&k3_clks dev clk> pair).
 */

int am67_tisci_get_freq(uint32_t dev, uint8_t clk, uint64_t *hz)
{
  uint16_t type = TISCI_MSG_GET_FREQ;
  uint32_t flags = TISCI_MSG_FLAG_AOP;
  uint8_t req[13];
  uint8_t resp[16];
  int ret;

  memset(req, 0, sizeof(req));
  memset(resp, 0, sizeof(resp));

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  /* Packed: header (type, host, seq, flags), device, clock */

  memcpy(req + 0, &type, 2);
  req[2] = TISCI_HOST_ID_MAIN_0_R5_1;
  req[3] = g_seq++;
  memcpy(req + 4, &flags, 4);
  memcpy(req + 8, &dev, 4);
  req[12] = clk;

  ret = sproxy_xfer(req, sizeof(req), resp, sizeof(resp));
  nxmutex_unlock(&g_lock);

  if (ret < 0)
    {
      return ret;
    }

  /* Response: header, then the frequency in Hz (64 bit) */

  memcpy(&flags, resp + 4, 4);
  if ((flags & TISCI_MSG_FLAG_ACK) == 0u)
    {
      return -EIO;
    }

  memcpy(hz, resp + 8, 8);
  return 0;
}

/* Read the state of device `id`: *programmed is what the hosts asked for,
 * *current what the hardware is in (0 off, 1 on, 2 in transition).  Works
 * for a device another host holds exclusively.
 */

int am67_tisci_get_device(uint32_t id, uint8_t *programmed,
                          uint8_t *current)
{
  uint16_t type = TISCI_MSG_GET_DEVICE;
  uint32_t flags = TISCI_MSG_FLAG_AOP;
  uint8_t req[12];
  uint8_t resp[18];
  int ret;

  memset(req, 0, sizeof(req));
  memset(resp, 0, sizeof(resp));

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  memcpy(req + 0, &type, 2);
  req[2] = TISCI_HOST_ID_MAIN_0_R5_1;
  req[3] = g_seq++;
  memcpy(req + 4, &flags, 4);
  memcpy(req + 8, &id, 4);

  ret = sproxy_xfer(req, sizeof(req), resp, sizeof(resp));
  nxmutex_unlock(&g_lock);

  if (ret < 0)
    {
      return ret;
    }

  /* Response: header, context loss count, resets, programmed, current */

  memcpy(&flags, resp + 4, 4);
  if ((flags & TISCI_MSG_FLAG_ACK) == 0u)
    {
      return -EIO;
    }

  *programmed = resp[16];
  *current = resp[17];
  return 0;
}
