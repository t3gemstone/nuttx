/****************************************************************************
 * arch/arm/src/am67/am67_sdhci.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/* MMCSD0 (eMMC) on the AM67: an SD Host Controller 4.10 with a TI PHY
 * wrapper, as an SDIO lower half for drivers/mmcsd.
 *
 * Deliberately simple: 4-bit bus, legacy timing at 20 MHz (no HS, no
 * tuning, PHY DLL off, delay chain on, tap values from the Linux DTS),
 * programmed I/O.  Commands are polled with a bounded wait; data blocks
 * move in the interrupt handler, which signals the end of the transfer.
 * Every wait has a time limit, so a dead card cannot hang the caller.
 *
 * Linux must leave the controller alone (px4-r5f overlay: sdhci0
 * disabled).  PHY setup follows drivers/mmc/host/sdhci_am654.c
 * (sdhci_am654_set_clock() with timing <= MMC_HS).
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include <nuttx/semaphore.h>

#include "arm_internal.h"
#include "am67_sdhci.h"

#ifdef CONFIG_AM67_SDHCI0

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SDHCI0_BASE           0x0fa10000u   /* SD host controller */
#define SDHCI0_SS_BASE        0x0fa18000u   /* TI config + PHY */
#define SDHCI0_TISCI_DEV      57u
#define SDHCI0_TISCI_CLK_XIN  2u
#define SDHCI0_IRQ            161           /* MMCSD0_EMMCSS_INTR_0 */

/* Standard SDHCI registers */

#define SDHCI_BLKSIZE         0x04  /* 16: block size, 16: block count */
#define SDHCI_ARG             0x08
#define SDHCI_XFERMODE        0x0c  /* 16: transfer mode, 16: command */
#define SDHCI_RESP0           0x10
#define SDHCI_RESP1           0x14
#define SDHCI_RESP2           0x18
#define SDHCI_RESP3           0x1c
#define SDHCI_DATA            0x20
#define SDHCI_PRSNT           0x24
#define SDHCI_HOSTCTL1        0x28  /* 8 */
#define SDHCI_PWRCTL          0x29  /* 8 */
#define SDHCI_CLKCTL          0x2c  /* 16 */
#define SDHCI_TOCTL           0x2e  /* 8 */
#define SDHCI_SWRST           0x2f  /* 8 */
#define SDHCI_INTSTAT         0x30  /* normal 15:0, error 31:16 */
#define SDHCI_INTSTATEN       0x34
#define SDHCI_INTSIGEN        0x38
#define SDHCI_HOSTCTL2        0x3e  /* 16 */
#define SDHCI_CAP1            0x40
#define SDHCI_VERSION         0xfe  /* 16 */

#define XFER_BLKCNTEN         (1u << 1)
#define XFER_READ             (1u << 4)
#define XFER_MULTI            (1u << 5)

#define CMD_RESP_NONE         (0u << 0)
#define CMD_RESP_136          (1u << 0)
#define CMD_RESP_48           (2u << 0)
#define CMD_RESP_48_BUSY      (3u << 0)
#define CMD_CRCCHK            (1u << 3)
#define CMD_IDXCHK            (1u << 4)
#define CMD_DATA              (1u << 5)
#define CMD_TYPE_ABORT        (3u << 6)
#define CMD_INDEX_SHIFT       8

#define PRSNT_CMDINHIBIT      (1u << 0)
#define PRSNT_DATINHIBIT      (1u << 1)
#define PRSNT_BUFWREN         (1u << 10)
#define PRSNT_BUFRDEN         (1u << 11)

#define HOSTCTL1_4BIT         (1u << 1)

#define PWRCTL_ON             (1u << 0)
#define PWRCTL_1V8            (5u << 1)
#define PWRCTL_3V3            (7u << 1)

#define CLK_INTEN             (1u << 0)
#define CLK_INTSTABLE         (1u << 1)
#define CLK_SDEN              (1u << 2)

#define SWRST_ALL             (1u << 0)
#define SWRST_CMD             (1u << 1)
#define SWRST_DAT             (1u << 2)

#define INT_CC                (1u << 0)   /* command complete */
#define INT_TC                (1u << 1)   /* transfer complete */
#define INT_BWR               (1u << 4)   /* buffer write ready */
#define INT_BRR               (1u << 5)   /* buffer read ready */
#define INT_ERR               (1u << 15)
#define INT_CTO               (1u << 16)  /* command timeout */
#define INT_CCRC              (1u << 17)
#define INT_CEB               (1u << 18)
#define INT_CIDX              (1u << 19)
#define INT_DTO               (1u << 20)  /* data timeout */
#define INT_DCRC              (1u << 21)
#define INT_DEB               (1u << 22)
#define INT_CMDERR            (INT_CTO | INT_CCRC | INT_CEB | INT_CIDX)
#define INT_DATAERR           (INT_DTO | INT_DCRC | INT_DEB)
#define INT_ALLERR            0x03ff0000u
#define INT_USED              (INT_CC | INT_TC | INT_BWR | INT_BRR | \
                               INT_ALLERR)

#define CAP1_BASECLK(c)       (((c) >> 8) & 0xffu)
#define CAP1_3V3              (1u << 24)

/* TI wrapper (sdhci_am654.c) */

#define SS_CTL_CFG_2          0x14
#define SS_SLOTTYPE_MASK      (3u << 30)
#define SS_SLOTTYPE_EMBEDDED  (1u << 30)
#define SS_PHY_CTRL1          0x100
#define SS_PHY_CTRL4          0x10c
#define SS_PHY_CTRL5          0x110
#define PHY1_ENDLL            (1u << 1)
#define PHY1_DR_TY_MASK       (7u << 20)  /* 0 = 50 ohm */
#define PHY4_OTAPDLYENA       (1u << 20)
#define PHY4_OTAPDLYSEL(n)    ((uint32_t)(n) << 12)
#define PHY4_OTAPDLYSEL_MASK  (15u << 12)
#define PHY4_ITAPCHGWIN       (1u << 9)
#define PHY4_ITAPDLYENA       (1u << 8)
#define PHY4_ITAPDLYSEL(n)    ((uint32_t)(n) << 0)
#define PHY4_ITAPDLYSEL_MASK  (31u << 0)
#define PHY5_SELDLYTXCLK      (1u << 17)
#define PHY5_SELDLYRXCLK      (1u << 16)
#define PHY5_CLKBUFSEL(n)     ((uint32_t)(n) << 0)
#define PHY5_CLKBUFSEL_MASK   (7u << 0)

/* Linux k3-am62p-j722s-common-main.dtsi, sdhci0, legacy timing */

#define SDHCI0_OTAP_LEGACY    0x1
#define SDHCI0_ITAP_LEGACY    0x10
#define SDHCI0_CLKBUF_SEL     0x7

/* Clocks and time limits */

#define SDHCI_IDMODE_HZ       400000u
#define SDHCI_TRANSFER_HZ     20000000u
#define SDHCI_CMD_TIMEOUT_US  100000u     /* command complete */
#define SDHCI_BUSY_TIMEOUT_US 1000000u    /* R1b busy (CMD6 up to ~1 s) */
#define SDHCI_RESET_TIMEOUT_US 100000u
#define SDHCI_CLK_TIMEOUT_US  150000u

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct am67_sdhci_s
{
  struct sdio_dev_s dev;        /* Must be first */
  uint32_t base;
  uint32_t ss;
  uint32_t baseclk;             /* Hz */

  /* Event wait */

  sem_t waitsem;
  volatile sdio_eventset_t waitevents;
  volatile sdio_eventset_t wkupevent;
  uint32_t waittimeout;         /* ms, 0 = none */

  /* Media callback (the eMMC never changes; stored for the API) */

  sdio_eventset_t cbevents;
  worker_t callback;
  void *cbarg;

  /* Current data transfer. The interrupt handler only signals; the thread
   * in sdhci_eventwait() moves the data and decides the result.
   */

  uint8_t *buffer;
  size_t remaining;
  bool reading;
  uint16_t blocksize;
  uint16_t nblocks;
  volatile bool piopending;     /* buffer ready, its interrupt masked */
  volatile bool tcpending;      /* transfer complete seen */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void sdhci_reset(struct sdio_dev_s *dev);
static sdio_capset_t sdhci_capabilities(struct sdio_dev_s *dev);
static sdio_statset_t sdhci_status(struct sdio_dev_s *dev);
static void sdhci_widebus(struct sdio_dev_s *dev, bool enable);
static void sdhci_clock(struct sdio_dev_s *dev, enum sdio_clock_e rate);
static int sdhci_attach(struct sdio_dev_s *dev);
static int sdhci_sendcmd(struct sdio_dev_s *dev, uint32_t cmd,
                         uint32_t arg);
static void sdhci_blocksetup(struct sdio_dev_s *dev, unsigned int blocklen,
                             unsigned int nblocks);
static int sdhci_recvsetup(struct sdio_dev_s *dev, uint8_t *buffer,
                           size_t nbytes);
static int sdhci_sendsetup(struct sdio_dev_s *dev, const uint8_t *buffer,
                           size_t nbytes);
static int sdhci_cancel(struct sdio_dev_s *dev);
static int sdhci_waitresponse(struct sdio_dev_s *dev, uint32_t cmd);
static int sdhci_recvshort(struct sdio_dev_s *dev, uint32_t cmd,
                           uint32_t *rshort);
static int sdhci_recvlong(struct sdio_dev_s *dev, uint32_t cmd,
                          uint32_t rlong[4]);
static void sdhci_waitenable(struct sdio_dev_s *dev,
                             sdio_eventset_t eventset, uint32_t timeout);
static sdio_eventset_t sdhci_eventwait(struct sdio_dev_s *dev);
static void sdhci_callbackenable(struct sdio_dev_s *dev,
                                 sdio_eventset_t eventset);
#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
static int sdhci_registercallback(struct sdio_dev_s *dev,
                                  worker_t callback, void *arg);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct am67_sdhci_s g_sdhci0 =
{
  .dev =
  {
    .reset            = sdhci_reset,
    .capabilities     = sdhci_capabilities,
    .status           = sdhci_status,
    .widebus          = sdhci_widebus,
    .clock            = sdhci_clock,
    .attach           = sdhci_attach,
    .sendcmd          = sdhci_sendcmd,
    .blocksetup       = sdhci_blocksetup,
    .recvsetup        = sdhci_recvsetup,
    .sendsetup        = sdhci_sendsetup,
    .cancel           = sdhci_cancel,
    .waitresponse     = sdhci_waitresponse,
    .recv_r1          = sdhci_recvshort,
    .recv_r2          = sdhci_recvlong,
    .recv_r3          = sdhci_recvshort,
    .recv_r4          = sdhci_recvshort,
    .recv_r5          = sdhci_recvshort,
    .recv_r6          = sdhci_recvshort,
    .recv_r7          = sdhci_recvshort,
    .waitenable       = sdhci_waitenable,
    .eventwait        = sdhci_eventwait,
    .callbackenable   = sdhci_callbackenable,
#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
    .registercallback = sdhci_registercallback,
#endif
  },
  .base = SDHCI0_BASE,
  .ss   = SDHCI0_SS_BASE,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t rd32(struct am67_sdhci_s *priv, uint32_t off)
{
  return getreg32(priv->base + off);
}

static inline void wr32(struct am67_sdhci_s *priv, uint32_t off,
                        uint32_t val)
{
  putreg32(val, priv->base + off);
}

static inline uint16_t rd16(struct am67_sdhci_s *priv, uint32_t off)
{
  return getreg16(priv->base + off);
}

static inline void wr16(struct am67_sdhci_s *priv, uint32_t off,
                        uint16_t val)
{
  putreg16(val, priv->base + off);
}

static inline uint8_t rd8(struct am67_sdhci_s *priv, uint32_t off)
{
  return getreg8(priv->base + off);
}

static inline void wr8(struct am67_sdhci_s *priv, uint32_t off,
                       uint8_t val)
{
  putreg8(val, priv->base + off);
}

static void ss_modify(struct am67_sdhci_s *priv, uint32_t off,
                      uint32_t clear, uint32_t set)
{
  uint32_t val = getreg32(priv->ss + off);

  putreg32((val & ~clear) | set, priv->ss + off);
}

/* Wait until (read & mask) == want, at most timeout_us.  Returns 0 or
 * -ETIMEDOUT.
 */

static int wait_reg8(struct am67_sdhci_s *priv, uint32_t off, uint8_t mask,
                     uint8_t want, uint32_t timeout_us)
{
  uint32_t t;

  for (t = 0; t < timeout_us; t += 10)
    {
      if ((rd8(priv, off) & mask) == want)
        {
          return OK;
        }

      up_udelay(10);
    }

  return (rd8(priv, off) & mask) == want ? OK : -ETIMEDOUT;
}

static void sdhci_softreset(struct am67_sdhci_s *priv, uint8_t what)
{
  wr8(priv, SDHCI_SWRST, what);
  if (wait_reg8(priv, SDHCI_SWRST, what, 0, SDHCI_RESET_TIMEOUT_US) < 0)
    {
      syslog(LOG_ERR, "[sdhci] reset 0x%02x stuck\n", what);
    }
}

/* Post the waiting thread if one of its events happened (from the
 * interrupt handler or with interrupts off).
 */

static void sdhci_endwait(struct am67_sdhci_s *priv,
                          sdio_eventset_t event)
{
  wr32(priv, SDHCI_INTSIGEN, 0);
  priv->buffer = NULL;
  priv->remaining = 0;

  if ((priv->waitevents & event) != 0)
    {
      priv->wkupevent = event;
      priv->waitevents = 0;
      nxsem_post(&priv->waitsem);
    }
}

#ifdef CONFIG_AM67_FAULT_INJECTION
static volatile unsigned int g_sdhci_inject_mode;
static volatile unsigned int g_sdhci_inject_count;

void am67_sdhci_inject_fault(unsigned int mode, unsigned int count)
{
  g_sdhci_inject_mode = mode;
  g_sdhci_inject_count = count;
}

/* Take one injected fault of this mode, if any is pending */

static bool sdhci_inject(unsigned int mode)
{
  if (g_sdhci_inject_count > 0 && g_sdhci_inject_mode == mode)
    {
      g_sdhci_inject_count--;
      return true;
    }

  return false;
}
#else
#  define sdhci_inject(mode) false
#endif

/* Move whole blocks between the buffer and the data port, in the waiting
 * thread with interrupts enabled. Each 512-byte block is 128 register
 * accesses (18-25 us); in the interrupt handler a multi-block write kept
 * interrupts off for up to ~1 ms. Works on local copies so that an error
 * ending the transfer meanwhile (sdhci_endwait() from the handler) cannot
 * pull the buffer from under it.
 */

static void sdhci_pio(struct am67_sdhci_s *priv)
{
  uint32_t bit;
  uint8_t *buffer;
  size_t remaining;
  bool reading;
  irqstate_t flags;

  /* Fault injection: drop the buffer-ready event, the wait times out */

  if (sdhci_inject(1))
    {
      return;
    }

  flags = enter_critical_section();
  buffer = priv->buffer;
  remaining = priv->remaining;
  reading = priv->reading;
  leave_critical_section(flags);

  bit = reading ? PRSNT_BUFRDEN : PRSNT_BUFWREN;

  while (buffer != NULL && remaining > 0 &&
         (rd32(priv, SDHCI_PRSNT) & bit) != 0)
    {
      size_t n = priv->blocksize < remaining ? priv->blocksize : remaining;
      size_t i;

      for (i = 0; i < n; i += 4)
        {
          uint32_t word;

          if (reading)
            {
              word = rd32(priv, SDHCI_DATA);
              memcpy(buffer + i, &word, 4);
            }
          else
            {
              memcpy(&word, buffer + i, 4);
              wr32(priv, SDHCI_DATA, word);
            }
        }

      buffer += n;
      remaining -= n;
    }

  /* Unless the transfer ended meanwhile, record the progress and let the
   * next buffer-ready interrupt through.
   */

  flags = enter_critical_section();
  if (priv->buffer != NULL)
    {
      priv->buffer = buffer;
      priv->remaining = remaining;

      if (remaining > 0)
        {
          wr32(priv, SDHCI_INTSIGEN, rd32(priv, SDHCI_INTSIGEN) |
                                     (reading ? INT_BRR : INT_BWR));
        }
    }

  leave_critical_section(flags);
}

static int sdhci_interrupt(int irq, void *context, void *arg)
{
  struct am67_sdhci_s *priv = arg;
  uint32_t pending = rd32(priv, SDHCI_INTSTAT) & rd32(priv, SDHCI_INTSIGEN);

  /* Buffer ready and transfer complete only wake the thread (see
   * sdhci_pio()). The completion is judged there, after the thread has
   * accounted for the last block.
   */

  if ((pending & (INT_BRR | INT_BWR)) != 0)
    {
      wr32(priv, SDHCI_INTSTAT, pending & (INT_BRR | INT_BWR));
      wr32(priv, SDHCI_INTSIGEN,
           rd32(priv, SDHCI_INTSIGEN) & ~(INT_BRR | INT_BWR));
      priv->piopending = true;
      nxsem_post(&priv->waitsem);
    }

  if ((pending & INT_DATAERR) != 0)
    {
      wr32(priv, SDHCI_INTSTAT, pending & INT_ALLERR);
      sdhci_endwait(priv, (pending & INT_DTO) != 0 ?
                    SDIOWAIT_TIMEOUT : SDIOWAIT_ERROR);
    }
  else if ((pending & INT_TC) != 0)
    {
      wr32(priv, SDHCI_INTSTAT, INT_TC);
      wr32(priv, SDHCI_INTSIGEN, rd32(priv, SDHCI_INTSIGEN) & ~INT_TC);
      priv->tcpending = true;
      nxsem_post(&priv->waitsem);
    }

  return OK;
}

/* SDCLK = base / (2 * N), N = 0 means base (SDHCI 3.0 10-bit divider) */

static void sdhci_setclock(struct am67_sdhci_s *priv, uint32_t hz)
{
  uint32_t div;
  uint16_t clk;

  /* PHY: DLL off before the clock changes */

  ss_modify(priv, SS_PHY_CTRL1, PHY1_ENDLL, 0);

  wr16(priv, SDHCI_CLKCTL, 0);
  if (hz == 0)
    {
      return;
    }

  if (hz >= priv->baseclk)
    {
      div = 0;
    }
  else
    {
      div = (priv->baseclk + 2 * hz - 1) / (2 * hz);
      if (div > 0x3ff)
        {
          div = 0x3ff;
        }
    }

  clk = (uint16_t)(((div & 0xff) << 8) | (((div >> 8) & 0x3) << 6));
  wr16(priv, SDHCI_CLKCTL, clk | CLK_INTEN);

  if (wait_reg8(priv, SDHCI_CLKCTL, CLK_INTSTABLE, CLK_INTSTABLE,
                SDHCI_CLK_TIMEOUT_US) < 0)
    {
      syslog(LOG_ERR, "[sdhci] internal clock not stable\n");
    }

  wr16(priv, SDHCI_CLKCTL, clk | CLK_INTEN | CLK_SDEN);

  /* PHY for legacy timing: output tap, delay chain with input tap,
   * clock buffer (sdhci_am654_set_clock(), timing <= MMC_HS).
   */

  ss_modify(priv, SS_PHY_CTRL4, PHY4_OTAPDLYENA | PHY4_OTAPDLYSEL_MASK,
            PHY4_OTAPDLYENA | PHY4_OTAPDLYSEL(SDHCI0_OTAP_LEGACY));
  ss_modify(priv, SS_PHY_CTRL5, 0, PHY5_SELDLYTXCLK | PHY5_SELDLYRXCLK);
  ss_modify(priv, SS_PHY_CTRL4, 0, PHY4_ITAPCHGWIN);
  ss_modify(priv, SS_PHY_CTRL4, PHY4_ITAPDLYENA | PHY4_ITAPDLYSEL_MASK,
            PHY4_ITAPDLYENA | PHY4_ITAPDLYSEL(SDHCI0_ITAP_LEGACY));
  ss_modify(priv, SS_PHY_CTRL4, PHY4_ITAPCHGWIN, 0);
  ss_modify(priv, SS_PHY_CTRL5, PHY5_CLKBUFSEL_MASK,
            PHY5_CLKBUFSEL(SDHCI0_CLKBUF_SEL));
}

/****************************************************************************
 * SDIO lower half
 ****************************************************************************/

static void sdhci_reset(struct sdio_dev_s *dev)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  irqstate_t flags;
  uint8_t pwr;
  uint32_t t;

  flags = enter_critical_section();
  priv->waitevents = 0;
  priv->wkupevent = 0;
  priv->buffer = NULL;
  priv->remaining = 0;
  leave_critical_section(flags);

  sdhci_softreset(priv, SWRST_ALL);

  /* eMMC slot, PHY: DLL off, 50 ohm driver */

  ss_modify(priv, SS_CTL_CFG_2, SS_SLOTTYPE_MASK, SS_SLOTTYPE_EMBEDDED);
  ss_modify(priv, SS_PHY_CTRL1, PHY1_ENDLL | PHY1_DR_TY_MASK, 0);

  /* Bus power: the eMMC supply is fixed on the board; the controller
   * still wants the power bit (it reads back once card detect settles).
   */

  pwr = ((rd32(priv, SDHCI_CAP1) & CAP1_3V3) != 0 ? PWRCTL_3V3 : PWRCTL_1V8);
  wr8(priv, SDHCI_PWRCTL, pwr);
  wr8(priv, SDHCI_PWRCTL, pwr | PWRCTL_ON);
  for (t = 0; t < 1500 && (rd8(priv, SDHCI_PWRCTL) & PWRCTL_ON) == 0; t++)
    {
      up_mdelay(1);
      wr8(priv, SDHCI_PWRCTL, pwr | PWRCTL_ON);
    }

  if ((rd8(priv, SDHCI_PWRCTL) & PWRCTL_ON) == 0)
    {
      syslog(LOG_ERR, "[sdhci] bus power did not come on\n");
    }

  wr8(priv, SDHCI_HOSTCTL1, 0);
  wr16(priv, SDHCI_HOSTCTL2, 0);
  wr8(priv, SDHCI_TOCTL, 0x0e);                 /* longest data timeout */
  wr32(priv, SDHCI_INTSTATEN, INT_USED);
  wr32(priv, SDHCI_INTSIGEN, 0);
  wr32(priv, SDHCI_INTSTAT, 0xffffffffu);

  sdhci_setclock(priv, SDHCI_IDMODE_HZ);
}

static sdio_capset_t sdhci_capabilities(struct sdio_dev_s *dev)
{
  /* The data must be staged before a write command is sent: the
   * controller starts the data phase with the command.
   */

  return SDIO_CAPS_4BIT_ONLY | SDIO_CAPS_DMABEFOREWRITE;
}

static sdio_statset_t sdhci_status(struct sdio_dev_s *dev)
{
  return SDIO_STATUS_PRESENT;                   /* soldered eMMC */
}

static void sdhci_widebus(struct sdio_dev_s *dev, bool enable)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  uint8_t val = rd8(priv, SDHCI_HOSTCTL1);

  wr8(priv, SDHCI_HOSTCTL1, enable ? (val | HOSTCTL1_4BIT) :
                                     (val & ~HOSTCTL1_4BIT));
}

static void sdhci_clock(struct sdio_dev_s *dev, enum sdio_clock_e rate)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;

  switch (rate)
    {
      case CLOCK_SDIO_DISABLED:
        sdhci_setclock(priv, 0);
        break;

      case CLOCK_IDMODE:
        sdhci_setclock(priv, SDHCI_IDMODE_HZ);
        break;

      default:
        sdhci_setclock(priv, SDHCI_TRANSFER_HZ);
        break;
    }
}

static int sdhci_attach(struct sdio_dev_s *dev)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  int ret;

  ret = irq_attach(SDHCI0_IRQ, sdhci_interrupt, priv);
  if (ret == OK)
    {
      up_enable_irq(SDHCI0_IRQ);
    }

  return ret;
}

static int sdhci_sendcmd(struct sdio_dev_s *dev, uint32_t cmd, uint32_t arg)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  uint32_t idx = cmd & MMCSD_CMDIDX_MASK;
  uint32_t inhibit = PRSNT_CMDINHIBIT;
  uint16_t cmdreg = (uint16_t)(idx << CMD_INDEX_SHIFT);
  uint16_t xfer = 0;
  uint32_t t;

  switch (cmd & MMCSD_RESPONSE_MASK)
    {
      case MMCSD_NO_RESPONSE:
        cmdreg |= CMD_RESP_NONE;
        break;

      case MMCSD_R1B_RESPONSE:
        cmdreg |= CMD_RESP_48_BUSY | CMD_CRCCHK | CMD_IDXCHK;
        inhibit |= PRSNT_DATINHIBIT;
        break;

      case MMCSD_R2_RESPONSE:
        cmdreg |= CMD_RESP_136 | CMD_CRCCHK;
        break;

      case MMCSD_R3_RESPONSE:
      case MMCSD_R4_RESPONSE:
        cmdreg |= CMD_RESP_48;
        break;

      default:                                  /* R1, R5, R6, R7 */
        cmdreg |= CMD_RESP_48 | CMD_CRCCHK | CMD_IDXCHK;
        break;
    }

  if ((cmd & MMCSD_DATAXFR_MASK) != 0)
    {
      cmdreg |= CMD_DATA;
      inhibit |= PRSNT_DATINHIBIT;
      xfer = XFER_BLKCNTEN;
      if ((cmd & MMCSD_WRXFR) == 0)
        {
          xfer |= XFER_READ;
        }

      if (priv->nblocks > 1)
        {
          xfer |= XFER_MULTI;
        }

      wr32(priv, SDHCI_BLKSIZE,
           priv->blocksize | ((uint32_t)priv->nblocks << 16));
    }

  if ((cmd & MMCSD_STOPXFR) != 0)
    {
      cmdreg |= CMD_TYPE_ABORT;
      inhibit = PRSNT_CMDINHIBIT;               /* stops a running transfer */
    }

  for (t = 0; (rd32(priv, SDHCI_PRSNT) & inhibit) != 0; t += 10)
    {
      if (t >= SDHCI_CMD_TIMEOUT_US)
        {
          syslog(LOG_ERR, "[sdhci] CMD%" PRIu32 ": line busy (0x%08" PRIx32
                 ")\n", idx, rd32(priv, SDHCI_PRSNT));
          sdhci_softreset(priv, SWRST_CMD | SWRST_DAT);
          return -EBUSY;
        }

      up_udelay(10);
    }

  wr32(priv, SDHCI_INTSTAT, INT_CC | INT_CMDERR);
  wr32(priv, SDHCI_ARG, arg);
  wr32(priv, SDHCI_XFERMODE, xfer | ((uint32_t)cmdreg << 16));
  return OK;
}

static void sdhci_blocksetup(struct sdio_dev_s *dev, unsigned int blocklen,
                             unsigned int nblocks)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;

  priv->blocksize = (uint16_t)blocklen;
  priv->nblocks = (uint16_t)nblocks;
}

static int sdhci_xfersetup(struct am67_sdhci_s *priv, uint8_t *buffer,
                           size_t nbytes, bool reading)
{
  irqstate_t flags;

  if (priv->blocksize == 0 || (priv->blocksize & 3) != 0 ||
      nbytes != (size_t)priv->blocksize * priv->nblocks)
    {
      return -EINVAL;
    }

  flags = enter_critical_section();
  priv->buffer = buffer;
  priv->remaining = nbytes;
  priv->reading = reading;
  priv->piopending = false;
  priv->tcpending = false;
  wr32(priv, SDHCI_INTSTAT, INT_TC | INT_BRR | INT_BWR | INT_DATAERR);
  wr32(priv, SDHCI_INTSIGEN, INT_TC | INT_DATAERR |
                             (reading ? INT_BRR : INT_BWR));
  leave_critical_section(flags);
  return OK;
}

static int sdhci_recvsetup(struct sdio_dev_s *dev, uint8_t *buffer,
                           size_t nbytes)
{
  return sdhci_xfersetup((struct am67_sdhci_s *)dev, buffer, nbytes, true);
}

static int sdhci_sendsetup(struct sdio_dev_s *dev, const uint8_t *buffer,
                           size_t nbytes)
{
  return sdhci_xfersetup((struct am67_sdhci_s *)dev, (uint8_t *)buffer,
                         nbytes, false);
}

static int sdhci_cancel(struct sdio_dev_s *dev)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  irqstate_t flags;

  flags = enter_critical_section();
  wr32(priv, SDHCI_INTSIGEN, 0);
  priv->buffer = NULL;
  priv->remaining = 0;
  priv->piopending = false;
  priv->tcpending = false;
  priv->waitevents = 0;
  priv->wkupevent = 0;
  leave_critical_section(flags);

  sdhci_softreset(priv, SWRST_CMD | SWRST_DAT);
  wr32(priv, SDHCI_INTSTAT, 0xffffffffu);
  return OK;
}

static int sdhci_waitresponse(struct sdio_dev_s *dev, uint32_t cmd)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  uint32_t stat = 0;
  uint32_t t;

  for (t = 0; ; t += 5)
    {
      stat = rd32(priv, SDHCI_INTSTAT);
      if ((stat & (INT_CC | INT_CMDERR)) != 0)
        {
          break;
        }

      if (t >= SDHCI_CMD_TIMEOUT_US)
        {
          syslog(LOG_ERR, "[sdhci] CMD%" PRIu32 ": no response\n",
                 cmd & MMCSD_CMDIDX_MASK);
          sdhci_softreset(priv, SWRST_CMD);
          return -ETIMEDOUT;
        }

      up_udelay(5);
    }

  if ((stat & INT_CMDERR) != 0)
    {
      wr32(priv, SDHCI_INTSTAT, INT_CMDERR | INT_CC);
      sdhci_softreset(priv, SWRST_CMD);
      return (stat & INT_CTO) != 0 ? -ETIMEDOUT : -EIO;
    }

  wr32(priv, SDHCI_INTSTAT, INT_CC);

  /* R1b without data: wait for the card to release DAT0 */

  if ((cmd & MMCSD_RESPONSE_MASK) == MMCSD_R1B_RESPONSE &&
      (cmd & MMCSD_DATAXFR_MASK) == 0)
    {
      for (t = 0; ; t += 10)
        {
          stat = rd32(priv, SDHCI_INTSTAT);
          if ((stat & (INT_TC | INT_DATAERR)) != 0)
            {
              break;
            }

          if (t >= SDHCI_BUSY_TIMEOUT_US)
            {
              syslog(LOG_ERR, "[sdhci] CMD%" PRIu32 ": busy too long\n",
                     cmd & MMCSD_CMDIDX_MASK);
              sdhci_softreset(priv, SWRST_CMD | SWRST_DAT);
              return -ETIMEDOUT;
            }

          up_udelay(10);
        }

      wr32(priv, SDHCI_INTSTAT, INT_TC | INT_DATAERR);
      if ((stat & INT_DATAERR) != 0)
        {
          sdhci_softreset(priv, SWRST_CMD | SWRST_DAT);
          return -EIO;
        }
    }

  return OK;
}

static int sdhci_recvshort(struct sdio_dev_s *dev, uint32_t cmd,
                           uint32_t *rshort)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;

  if (rshort != NULL)
    {
      *rshort = rd32(priv, SDHCI_RESP0);
    }

  return OK;
}

/* The controller drops the CRC byte: RESP3..0 hold R2 bits 127:8, shifted
 * down by 8.  drivers/mmcsd wants bits 127:0 in rlong[0..3].
 */

static int sdhci_recvlong(struct sdio_dev_s *dev, uint32_t cmd,
                          uint32_t rlong[4])
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  uint32_t r3 = rd32(priv, SDHCI_RESP3);
  uint32_t r2 = rd32(priv, SDHCI_RESP2);
  uint32_t r1 = rd32(priv, SDHCI_RESP1);
  uint32_t r0 = rd32(priv, SDHCI_RESP0);

  if (rlong != NULL)
    {
      rlong[0] = (r3 << 8) | (r2 >> 24);
      rlong[1] = (r2 << 8) | (r1 >> 24);
      rlong[2] = (r1 << 8) | (r0 >> 24);
      rlong[3] = r0 << 8;
    }

  return OK;
}

static void sdhci_waitenable(struct sdio_dev_s *dev,
                             sdio_eventset_t eventset, uint32_t timeout)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  irqstate_t flags;

  flags = enter_critical_section();
  priv->waitevents = eventset;
  priv->wkupevent = 0;
  priv->waittimeout = timeout;
  nxsem_reset(&priv->waitsem, 0);
  leave_critical_section(flags);
}

static sdio_eventset_t sdhci_eventwait(struct sdio_dev_s *dev)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;
  sdio_eventset_t event;
  irqstate_t flags;
  clock_t deadline;
  clock_t now;
  bool pio;
  bool tc;
  int ret = OK;

  /* Always bounded: a missing interrupt must not hang the file system */

  deadline = clock_systime_ticks() +
             MSEC2TICK(priv->waittimeout != 0 ? priv->waittimeout : 1000);

  for (; ; )
    {
      flags = enter_critical_section();
      event = priv->wkupevent;
      pio = priv->piopending;
      tc = priv->tcpending;
      priv->piopending = false;
      priv->tcpending = false;

      /* All data is in (reads) or taken by the card (writes) once the
       * controller reports transfer complete; the block count confirms it.
       */

      if (event == 0 && tc && !pio)
        {
          sdhci_endwait(priv, priv->remaining == 0 && !sdhci_inject(2) ?
                        SDIOWAIT_TRANSFERDONE : SDIOWAIT_ERROR);
          event = priv->wkupevent;
        }
      else if (tc)
        {
          priv->tcpending = true;   /* judge it after this block */
        }

      if (event == 0 && ret < 0)
        {
          priv->wkupevent = SDIOWAIT_TIMEOUT;
          priv->waitevents = 0;
          wr32(priv, SDHCI_INTSIGEN, 0);
          priv->buffer = NULL;
          priv->remaining = 0;
          event = SDIOWAIT_TIMEOUT;
        }

      leave_critical_section(flags);

      if (event != 0)
        {
          break;
        }

      if (pio)
        {
          sdhci_pio(priv);
          continue;
        }

      now = clock_systime_ticks();
      ret = now < deadline ?
            nxsem_tickwait_uninterruptible(&priv->waitsem, deadline - now) :
            -ETIMEDOUT;
    }

  if ((event & (SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR)) != 0)
    {
      sdhci_softreset(priv, SWRST_CMD | SWRST_DAT);
    }

  return event;
}

static void sdhci_callbackenable(struct sdio_dev_s *dev,
                                 sdio_eventset_t eventset)
{
  ((struct am67_sdhci_s *)dev)->cbevents = eventset;
}

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
static int sdhci_registercallback(struct sdio_dev_s *dev,
                                  worker_t callback, void *arg)
{
  struct am67_sdhci_s *priv = (struct am67_sdhci_s *)dev;

  priv->callback = callback;
  priv->cbarg = arg;
  return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int am67_tisci_device_on(uint32_t id);
int am67_tisci_get_freq(uint32_t dev, uint8_t clk, uint64_t *hz);
int am67_tisci_get_device(uint32_t id, uint8_t *programmed,
                          uint8_t *current);

struct sdio_dev_s *am67_sdhci0_initialize(void)
{
  struct am67_sdhci_s *priv = &g_sdhci0;
  uint64_t xin = 0;
  uint32_t cap;
  uint16_t ver;

  if (am67_tisci_device_on(SDHCI0_TISCI_DEV) < 0)
    {
      uint8_t programmed = 0;
      uint8_t current = 0;

      /* NAK: another host holds the device exclusively.  U-Boot takes
       * MMCSD0 that way and never gives it back, and Linux (sdhci0
       * disabled) never touches it.  If the hardware is on, the registers
       * work; only its power state cannot be changed from here.  If it is
       * off, touching a register would abort: give up.
       */

      if (am67_tisci_get_device(SDHCI0_TISCI_DEV, &programmed,
                                &current) < 0 ||
          current != 1)
        {
          syslog(LOG_ERR, "[sdhci] MMCSD0 not ours and not on (state %u/%u)"
                 "\n", programmed, current);
          return NULL;
        }

      syslog(LOG_WARNING, "[sdhci] MMCSD0 held by another host, already on"
             " (programmed %u); using it as it is\n", programmed);
    }

  ver = rd16(priv, SDHCI_VERSION);
  cap = rd32(priv, SDHCI_CAP1);
  (void)am67_tisci_get_freq(SDHCI0_TISCI_DEV, SDHCI0_TISCI_CLK_XIN, &xin);

  /* The capability field is the base clock in MHz; the TISCI value is
   * what the clock tree really delivers.  Trust the clock tree.
   */

  priv->baseclk = xin != 0 ? (uint32_t)xin : CAP1_BASECLK(cap) * 1000000u;
  syslog(LOG_INFO, "[sdhci] MMCSD0 version 0x%04x cap 0x%08" PRIx32
         " xin %" PRIu32 " Hz (cap %" PRIu32 " MHz)\n", ver, cap,
         (uint32_t)xin, CAP1_BASECLK(cap));

  if (ver == 0 || ver == 0xffff || priv->baseclk == 0)
    {
      return NULL;
    }

  nxmutex_init(&priv->dev.mutex);
  nxsem_init(&priv->waitsem, 0, 0);
  sdhci_reset(&priv->dev);
  return &priv->dev;
}

#endif /* CONFIG_AM67_SDHCI0 */
