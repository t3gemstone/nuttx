/****************************************************************************
 * arch/arm/src/am67/am67_mcspi.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/irq.h>
#include <nuttx/semaphore.h>
#include <nuttx/spi/spi.h>
#include <syslog.h>

#include "am67_mcspi.h"
#include "am67_pinmux.h"
#include "am67_tisci.h"
#include "arm.h"
#include "arm_internal.h"
#include "sctlr.h"

#ifdef CONFIG_AM67_MCSPI0

/****************************************************************************
 * Private Function Prototypes (board-provided)
 ****************************************************************************/

void am67_spi0select(FAR struct spi_dev_s *dev, uint32_t devid,
                     bool selected);
uint8_t am67_spi0status(FAR struct spi_dev_s *dev, uint32_t devid);

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AM67_MCSPI_MAX_DIVIDER      4096u

/* Longest wait for one status bit. One byte at the slowest clock in use
 * (1 MHz) takes 8 us. Measured with the PMU cycle counter; the R5F runs at
 * 800 MHz (799.93 MHz measured on the O1).
 */

#define AM67_MCSPI_CPU_HZ           800000000u
#define AM67_MCSPI_TIMEOUT_US       100u
#define AM67_MCSPI_TIMEOUT_CYCLES   (AM67_MCSPI_TIMEOUT_US * \
                                     (AM67_MCSPI_CPU_HZ / 1000000u))

/* Backstop in case the cycle counter ever stops: each poll is at least one
 * MCU-domain register read, so this is well above the time limit.
 */

#define AM67_MCSPI_POLL_MAX         1000000u

/* FIFO transfers longer than one FIFO half are driven by the interrupt:
 * the receive half raises RXi_FULL every AM67_MCSPI_IRQ_BLOCK bytes and
 * the handler moves exactly that many bytes each way; the tail is read on
 * end of word count. The thread sleeps in between instead of polling for
 * the wire time.
 */

#define AM67_MCSPI_IRQ_BLOCK        16u

/* Wait for an interrupt-driven transfer. The longest one (FIFO read of
 * the ICM-20948, 267 bytes) takes 0.4 ms; the system tick is 10 ms.
 */

#define AM67_MCSPI_IRQ_TIMEOUT_TICKS 3

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* A FIFO transfer in progress, shared with the interrupt handler */

struct am67_mcspi_xfer_s
{
  FAR const uint8_t *tx8;
  FAR uint8_t       *rx8;
  size_t             nwords;
  size_t             tx;       /* words written to TX */
  size_t             rx;       /* words read from RX */
  volatile bool      active;   /* the interrupt handler owns the transfer */
  volatile bool      ok;       /* result, valid once !active */
};

struct am67_mcspi_dev_s
{
  struct spi_dev_s spidev;
  uint32_t         base;
  mutex_t          lock;
  uint8_t          channel;
  uint32_t         frequency;
  uint8_t          mode;
  uint8_t          nbits;
  uint32_t         chconf;
  uint32_t         chctrl;
  bool             selected;
  bool             shifting;   /* words shifted since the last EOT wait */
  uint32_t         errors;     /* Failures since am67_mcspi_take_errors() */
  uint32_t         reset_at;   /* errors when the controller was last reset */
  struct am67_mcspi_xfer_s xfer;
  sem_t            xfer_sem;   /* posted when an interrupt transfer ends */
  bool             irq_ready;  /* interrupt attached */
  struct am67_mcspi_stats_s stats;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int       am67_mcspi_lock(FAR struct spi_dev_s *dev, bool lock);
static uint32_t  am67_mcspi_setfrequency(FAR struct spi_dev_s *dev,
                                         uint32_t frequency);
static void      am67_mcspi_setmode(FAR struct spi_dev_s *dev,
                                    enum spi_mode_e mode);
static void      am67_mcspi_setbits(FAR struct spi_dev_s *dev, int nbits);
static uint32_t  am67_mcspi_send(FAR struct spi_dev_s *dev, uint32_t wd);
#ifdef CONFIG_SPI_EXCHANGE
static void      am67_mcspi_exchange(FAR struct spi_dev_s *dev,
                                     FAR const void *txbuffer,
                                     FAR void *rxbuffer, size_t nwords);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct spi_ops_s g_am67_spi0ops =
{
  .lock         = am67_mcspi_lock,
  .select       = am67_spi0select,
  .setfrequency = am67_mcspi_setfrequency,
  .setmode      = am67_mcspi_setmode,
  .setbits      = am67_mcspi_setbits,
  .status       = am67_spi0status,
  .send         = am67_mcspi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange     = am67_mcspi_exchange,
#endif
};

#ifdef CONFIG_AM67_FAULT_INJECTION
static volatile unsigned int g_am67_mcspi_inject;
#endif

static struct am67_mcspi_dev_s g_spi0dev =
{
  .spidev =
    {
      &g_am67_spi0ops
    },
  .base       = AM67_MCSPI0_BASE,
  .lock       = NXMUTEX_INITIALIZER,
  .xfer_sem   = SEM_INITIALIZER(0),
  .channel    = 0,
  .frequency  = 1000000,
  .mode       = SPIDEV_MODE3,
  .nbits      = 8,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t am67_mcspi_getreg(uint32_t base, uint32_t offset)
{
  return getreg32(base + offset);
}

static inline void am67_mcspi_putreg(uint32_t base, uint32_t offset,
                                     uint32_t value)
{
  putreg32(value, base + offset);
}

static inline FAR struct am67_mcspi_dev_s *
am67_mcspi_dev(FAR struct spi_dev_s *dev)
{
  return (FAR struct am67_mcspi_dev_s *)dev;
}

static bool am67_mcspi_waitreg(uint32_t addr, uint32_t mask)
{
  const uint32_t start = (uint32_t)up_perf_gettime();
  uint32_t polls = 0;

  while ((getreg32(addr) & mask) == 0)
    {
      if ((uint32_t)up_perf_gettime() - start > AM67_MCSPI_TIMEOUT_CYCLES ||
          ++polls >= AM67_MCSPI_POLL_MAX)
        {
          return false;
        }
    }

  return true;
}

static bool am67_mcspi_waitstat(uint32_t base, uint8_t channel,
                                uint32_t mask)
{
  return am67_mcspi_waitreg(base + AM67_MCSPI_CHSTAT0 +
                            AM67_MCSPI_CH_OFFSET(channel), mask);
}

static void am67_mcspi_channel_enable(FAR struct am67_mcspi_dev_s *priv,
                                      bool enable)
{
  uint32_t chctrl = priv->chctrl;

  if (enable)
    {
      chctrl |= AM67_MCSPI_CHCTRL_EN;
    }
  else
    {
      chctrl &= ~AM67_MCSPI_CHCTRL_EN;
    }

  priv->chctrl = chctrl;
  am67_mcspi_putreg(priv->base,
                    AM67_MCSPI_CHCTRL0 + AM67_MCSPI_CH_OFFSET(priv->channel),
                    chctrl);
}

static void am67_mcspi_cs_force(FAR struct am67_mcspi_dev_s *priv,
                                bool deassert)
{
  uint32_t chconf = priv->chconf;

  if (deassert)
    {
      /* Wait for EOT so CS isn't dropped mid-word at high SCLK, which
       * truncates writes (e.g. the ICM-20948 bank-switch).
       */

      if (priv->shifting &&
          !am67_mcspi_waitstat(priv->base, priv->channel,
                               AM67_MCSPI_CHSTAT_EOT))
        {
          priv->errors++;
        }

      priv->shifting = false;
      chconf &= ~AM67_MCSPI_CHCONF_FORCE;  /* FORCE=0: CS deasserted (high) */
    }
  else
    {
      chconf |= AM67_MCSPI_CHCONF_FORCE;   /* FORCE=1: CS asserted (low, EPOL=1) */
    }

  priv->chconf = chconf;
  am67_mcspi_putreg(priv->base,
                    AM67_MCSPI_CHCONF0 + AM67_MCSPI_CH_OFFSET(priv->channel),
                    chconf);

  /* Keep the channel enabled between transfers; toggling it re-inits the
   * shift logic and can drop the next transfer's first word. CS is driven
   * by FORCE above, so an idle enabled channel generates no clock.
   */

  if (!deassert)
    {
      am67_mcspi_channel_enable(priv, true);
    }
}

static uint8_t am67_mcspi_calc_divisor(uint32_t speed_hz,
                                       uint32_t ref_clk_hz)
{
  uint8_t clkd;

  for (clkd = 0; clkd < 15; clkd++)
    {
      if (speed_hz >= (ref_clk_hz >> (clkd + 1)))
        {
          break;
        }
    }

  return clkd;
}

static void am67_mcspi_apply_hwconfig(FAR struct am67_mcspi_dev_s *priv)
{
  uint32_t ref_clk = CONFIG_AM67_MCSPI0_FCLK;
  uint32_t speed_hz = priv->frequency;
  uint32_t chconf;
  uint32_t chctrl = priv->chctrl & AM67_MCSPI_CHCTRL_EN; /* preserve EN bit */
  uint8_t clkd;
  uint32_t div;
  uint32_t extclk = 0;

  if (speed_hz > ref_clk)
    {
      speed_hz = ref_clk;
    }

  if (speed_hz < (ref_clk / AM67_MCSPI_MAX_DIVIDER))
    {
      clkd = am67_mcspi_calc_divisor(speed_hz, ref_clk);
      speed_hz = ref_clk >> (clkd + 1);
      chconf = 0;
      chctrl &= ~AM67_MCSPI_CHCTRL_EXTCLK_MASK;
    }
  else
    {
      div = (ref_clk + speed_hz - 1) / speed_hz;
      speed_hz = ref_clk / div;
      clkd = (div - 1) & 0x0fu;
      extclk = (div - 1) >> 4;
      chconf = AM67_MCSPI_CHCONF_CLKG;
      chctrl &= ~AM67_MCSPI_CHCTRL_EXTCLK_MASK;
      chctrl |= (extclk << AM67_MCSPI_CHCTRL_EXTCLK_SHIFT) &
                AM67_MCSPI_CHCTRL_EXTCLK_MASK;
    }

  priv->frequency = speed_hz;

  chconf |= AM67_MCSPI_CHCONF_IS;    /* IS=1: receive from D1 (MISO) */
  chconf &= ~AM67_MCSPI_CHCONF_DPE0; /* DPE0=0: TX enabled on D0 (MOSI) */
  chconf |= AM67_MCSPI_CHCONF_DPE1;  /* DPE1=1: TX disabled on D1 */
  chconf |= AM67_MCSPI_CHCONF_EPOL;  /* EPOL=1: CS active-low */
  chconf &= ~AM67_MCSPI_CHCONF_WL_MASK;
  chconf |= ((uint32_t)(priv->nbits - 1) << AM67_MCSPI_CHCONF_WL_SHIFT);
  chconf &= ~AM67_MCSPI_CHCONF_CLKD_MASK;
  chconf |= ((uint32_t)clkd << AM67_MCSPI_CHCONF_CLKD_SHIFT);

  if ((priv->mode & SPIDEV_MODE2) != 0)
    {
      chconf |= AM67_MCSPI_CHCONF_POL;
    }
  else
    {
      chconf &= ~AM67_MCSPI_CHCONF_POL;
    }

  if ((priv->mode & SPIDEV_MODE1) != 0)
    {
      chconf |= AM67_MCSPI_CHCONF_PHA;
    }
  else
    {
      chconf &= ~AM67_MCSPI_CHCONF_PHA;
    }

  /* SPIENSLV: route this channel to its natural CS pin (chN uses CSN) */

  chconf &= ~AM67_MCSPI_CHCONF_SPIENSLV_MASK;
  chconf |= ((uint32_t)priv->channel << AM67_MCSPI_CHCONF_SPIENSLV_SHIFT) &
             AM67_MCSPI_CHCONF_SPIENSLV_MASK;

  /* Re-assert FORCE if the channel is currently selected so that
   * reconfiguring mid-transaction does not glitch the CS line.
   */

  if (priv->selected)
    {
      chconf |= AM67_MCSPI_CHCONF_FORCE;
    }

  priv->chconf = chconf;
  priv->chctrl = chctrl;

  am67_mcspi_putreg(priv->base,
                    AM67_MCSPI_CHCONF0 + AM67_MCSPI_CH_OFFSET(priv->channel),
                    chconf);
  am67_mcspi_putreg(priv->base,
                    AM67_MCSPI_CHCTRL0 + AM67_MCSPI_CH_OFFSET(priv->channel),
                    chctrl);

  if (priv->selected)
    {
      am67_mcspi_channel_enable(priv, true);
    }
}

static void am67_mcspi_clear_other_force(FAR struct am67_mcspi_dev_s *priv,
                                        uint8_t keep)
{
  uint8_t ch;

  /* SINGLE=1 allows FORCE on only one channel. Clearing CHCTRL.EN on
   * the channel we leave does not clear its FORCE bit, so that CS can
   * stay asserted while the new channel is selected.
   */

  for (ch = 0; ch < AM67_MCSPI_NCHANNELS; ch++)
    {
      uint32_t off;
      uint32_t conf;

      if (ch == keep)
        {
          continue;
        }

      off = AM67_MCSPI_CHCONF0 + AM67_MCSPI_CH_OFFSET(ch);
      conf = am67_mcspi_getreg(priv->base, off);

      if ((conf & AM67_MCSPI_CHCONF_FORCE) == 0)
        {
          continue;
        }

      conf &= ~AM67_MCSPI_CHCONF_FORCE;
      am67_mcspi_putreg(priv->base, off, conf);

      if (ch == priv->channel)
        {
          priv->chconf &= ~AM67_MCSPI_CHCONF_FORCE;
        }
    }
}

static void am67_mcspi_select_channel(FAR struct am67_mcspi_dev_s *priv,
                                      uint8_t channel)
{
  if (priv->channel == channel)
    {
      return;
    }

  am67_mcspi_channel_enable(priv, false);
  priv->channel = channel;
  am67_mcspi_apply_hwconfig(priv);
}

static void am67_mcspi_controller_init(FAR struct am67_mcspi_dev_s *priv)
{
  uint32_t regval;
  uint32_t modulctrl;
  uint8_t ch;

  /* The status waits time out on the PMU cycle counter; make sure it
   * counts (idempotent, same setup as up_perf_init()).
   */

  cp15_pmu_pmcr(PMCR_E);
  cp15_pmu_cesr(PMCESR_CCES);

  /* Set HL_SYSCONFIG to no-idle so the OCP interconnect does not gate
   * the functional clock while we poll status registers.
   */

  am67_mcspi_putreg(priv->base, AM67_MCSPI_HL_SYSCONFIG,
                    AM67_MCSPI_HL_SYSCONFIG_NOIDLE);

  /* Trigger the hardware soft reset */

  regval = am67_mcspi_getreg(priv->base, AM67_MCSPI_SYSCONFIG);
  regval |= AM67_MCSPI_SYSCONFIG_SOFTRESET;
  am67_mcspi_putreg(priv->base, AM67_MCSPI_SYSCONFIG, regval);

  /* Wait until SYSSTATUS.RESETDONE is 1. On timeout carry on: the
   * transfers then time out and are reported as errors.
   */

  if (!am67_mcspi_waitreg(priv->base + AM67_MCSPI_SYSSTATUS,
                          AM67_MCSPI_SYSSTATUS_RESETDONE))
    {
      spierr("ERROR: soft reset timeout\n");
      priv->errors++;
    }

  /* Configure CLOCKACTIVITY and SIDLEMODE */

  regval = AM67_MCSPI_SYSCONFIG_CLKACT_BOTH |
           AM67_MCSPI_SYSCONFIG_SIDLEMODE_NO;
  am67_mcspi_putreg(priv->base, AM67_MCSPI_SYSCONFIG, regval);

  /* SINGLE=1: one channel active at a time; CS controlled via FORCE bit */

  modulctrl = AM67_MCSPI_MODULCTRL_SINGLE;
  am67_mcspi_putreg(priv->base, AM67_MCSPI_MODULCTRL, modulctrl);

  /* After reset every CHCONF has EPOL=0, which drives an idle CS low.
   * The devices on this bus are active-low, so an unused channel would
   * keep its device selected and let it drive MISO against the one
   * being read. Park all four chip-selects high before any pad is
   * muxed to the controller.
   */

  for (ch = 0; ch < AM67_MCSPI_NCHANNELS; ch++)
    {
      am67_mcspi_putreg(priv->base,
                        AM67_MCSPI_CHCONF0 + AM67_MCSPI_CH_OFFSET(ch),
                        AM67_MCSPI_CHCONF_EPOL | AM67_MCSPI_CHCONF_IS |
                        AM67_MCSPI_CHCONF_DPE1);
    }

  am67_mcspi_apply_hwconfig(priv);
}

/* Shift one word. On a timeout count the error and return false; the
 * caller stops the transfer, since every later word would time out too.
 */

static bool am67_mcspi_transfer_word(FAR struct am67_mcspi_dev_s *priv,
                                     uint32_t wd, FAR uint32_t *rd)
{
  uint32_t choff = AM67_MCSPI_CH_OFFSET(priv->channel);

  if (!am67_mcspi_waitstat(priv->base, priv->channel,
                           AM67_MCSPI_CHSTAT_TXS))
    {
      spierr("ERROR: TX timeout ch%u\n", priv->channel);
      priv->errors++;
      return false;
    }

  am67_mcspi_putreg(priv->base, AM67_MCSPI_TX0 + choff, wd);
  priv->shifting = true;

  if (!am67_mcspi_waitstat(priv->base, priv->channel,
                           AM67_MCSPI_CHSTAT_RXS))
    {
      spierr("ERROR: RX timeout ch%u\n", priv->channel);
      priv->errors++;
      return false;
    }

  *rd = am67_mcspi_getreg(priv->base, AM67_MCSPI_RX0 + choff);
  return true;
}

/****************************************************************************
 * SPI driver methods
 ****************************************************************************/

static int am67_mcspi_lock(FAR struct spi_dev_s *dev, bool lock)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);

  if (lock)
    {
      return nxmutex_lock(&priv->lock);
    }

  nxmutex_unlock(&priv->lock);
  return OK;
}

static uint32_t am67_mcspi_setfrequency(FAR struct spi_dev_s *dev,
                                        uint32_t frequency)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  uint32_t start = (uint32_t)up_perf_gettime();

  priv->frequency = frequency;
  am67_mcspi_apply_hwconfig(priv);
  priv->stats.config_cycles += (uint32_t)up_perf_gettime() - start;
  return priv->frequency;
}

static void am67_mcspi_setmode(FAR struct spi_dev_s *dev,
                               enum spi_mode_e mode)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  uint32_t start = (uint32_t)up_perf_gettime();

  priv->mode = (uint8_t)mode;
  am67_mcspi_apply_hwconfig(priv);
  priv->stats.config_cycles += (uint32_t)up_perf_gettime() - start;
}

static void am67_mcspi_setbits(FAR struct spi_dev_s *dev, int nbits)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  uint32_t start = (uint32_t)up_perf_gettime();

  DEBUGASSERT(nbits == 8 || nbits == 16);
  priv->nbits = (uint8_t)nbits;
  am67_mcspi_apply_hwconfig(priv);
  priv->stats.config_cycles += (uint32_t)up_perf_gettime() - start;
}

static uint32_t am67_mcspi_send(FAR struct spi_dev_s *dev, uint32_t wd)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  uint32_t start = (uint32_t)up_perf_gettime();
  uint32_t rd = 0;

  if (!priv->selected)
    {
      priv->errors++;
      return 0;
    }

  am67_mcspi_transfer_word(priv, wd, &rd);
  priv->stats.words++;
  priv->stats.xfer_cycles += (uint32_t)up_perf_gettime() - start;
  return rd;
}

#ifdef CONFIG_SPI_EXCHANGE
static inline void am67_mcspi_fifo_push(FAR struct am67_mcspi_dev_s *priv,
                                        uint32_t txreg)
{
  FAR struct am67_mcspi_xfer_s *x = &priv->xfer;

  putreg32(x->tx8 != NULL ? x->tx8[x->tx] : 0xffu, txreg);
  x->tx++;
}

static inline void am67_mcspi_fifo_pop(FAR struct am67_mcspi_dev_s *priv,
                                       uint32_t rxreg)
{
  FAR struct am67_mcspi_xfer_s *x = &priv->xfer;
  uint32_t rd = getreg32(rxreg);

  if (x->rx8 != NULL)
    {
      x->rx8[x->rx] = (uint8_t)rd;
    }

  x->rx++;
}

/* Receive until every word is in, queueing one more word per word
 * received. Only the receive status is polled: at most one FIFO half
 * (32 bytes) is in flight, so the transmit half never fills and the
 * receive half cannot overflow. In turbo mode the last word is not moved
 * into the FIFO: it stays in RX0 with RXS set and RXFFE still set (seen on
 * the O1, 2026-09-28; the Linux omap2-mcspi driver reads it the same way).
 * Returns false if no word arrived for AM67_MCSPI_TIMEOUT_US.
 */

static bool am67_mcspi_fifo_drain(FAR struct am67_mcspi_dev_s *priv)
{
  FAR struct am67_mcspi_xfer_s *x = &priv->xfer;
  const uint32_t choff = AM67_MCSPI_CH_OFFSET(priv->channel);
  const uint32_t stat = priv->base + AM67_MCSPI_CHSTAT0 + choff;
  const uint32_t txreg = priv->base + AM67_MCSPI_TX0 + choff;
  const uint32_t rxreg = priv->base + AM67_MCSPI_RX0 + choff;
  uint32_t progress = (uint32_t)up_perf_gettime();

  while (x->rx < x->nwords)
    {
      uint32_t st = getreg32(stat);

      if ((st & AM67_MCSPI_CHSTAT_RXFFE) == 0 ||
          (x->rx == x->nwords - 1 && (st & AM67_MCSPI_CHSTAT_RXS) != 0))
        {
          am67_mcspi_fifo_pop(priv, rxreg);

          if (x->tx < x->nwords)
            {
              am67_mcspi_fifo_push(priv, txreg);
            }

          progress = (uint32_t)up_perf_gettime();
        }
      else if ((uint32_t)up_perf_gettime() - progress >
               AM67_MCSPI_TIMEOUT_CYCLES)
        {
          priv->stats.fifo_stalls++;
          priv->stats.fail_stat = st;
          priv->stats.fail_rx = x->rx;
          return false;
        }
    }

  return true;
}

/* MCU_MCSPI0 interrupt: RXi_FULL guarantees AM67_MCSPI_IRQ_BLOCK words in
 * the receive FIFO, and the next one is raised only after exactly that
 * many reads. End of word count means every word has been shifted: read
 * the rest and wake the thread.
 */

static int am67_mcspi_interrupt(int irq, FAR void *context, FAR void *arg)
{
  FAR struct am67_mcspi_dev_s *priv = arg;
  FAR struct am67_mcspi_xfer_s *x = &priv->xfer;
  const uint32_t choff = AM67_MCSPI_CH_OFFSET(priv->channel);
  const uint32_t txreg = priv->base + AM67_MCSPI_TX0 + choff;
  const uint32_t rxreg = priv->base + AM67_MCSPI_RX0 + choff;
  uint32_t status = am67_mcspi_getreg(priv->base, AM67_MCSPI_IRQSTATUS);
  unsigned i;

  am67_mcspi_putreg(priv->base, AM67_MCSPI_IRQSTATUS, status);

  if (!x->active)
    {
      am67_mcspi_putreg(priv->base, AM67_MCSPI_IRQENABLE, 0);
      return OK;
    }

  if ((status & AM67_MCSPI_IRQ_RX_FULL(priv->channel)) != 0)
    {
      /* The last word never enters the FIFO, so a full block always ends
       * before it.
       */

      for (i = 0; i < AM67_MCSPI_IRQ_BLOCK && x->rx < x->nwords - 1; i++)
        {
          am67_mcspi_fifo_pop(priv, rxreg);

          if (x->tx < x->nwords)
            {
              am67_mcspi_fifo_push(priv, txreg);
            }
        }
    }

  if ((status & AM67_MCSPI_IRQ_EOW) != 0)
    {
      x->ok = am67_mcspi_fifo_drain(priv);
      am67_mcspi_putreg(priv->base, AM67_MCSPI_IRQENABLE, 0);
      x->active = false;
      nxsem_post(&priv->xfer_sem);
    }

  return OK;
}

/* The interrupt path needs a thread that may sleep: not an interrupt
 * handler and not a caller that masked IRQs (e.g. a LOCK_PREEMPTION
 * device).
 */

static bool am67_mcspi_can_sleep(FAR struct am67_mcspi_dev_s *priv)
{
  irqstate_t flags;

  if (!priv->irq_ready || up_interrupt_context())
    {
      return false;
    }

  flags = up_irq_save();
  up_irq_restore(flags);
  return (flags & PSR_I_BIT) == 0;
}

/* Wait for the interrupt handler to finish the transfer. On a timeout
 * take the transfer back: mask the interrupt, then drop a completion that
 * may have been posted meanwhile so it cannot end the next transfer.
 */

static bool am67_mcspi_fifo_wait(FAR struct am67_mcspi_dev_s *priv)
{
  FAR struct am67_mcspi_xfer_s *x = &priv->xfer;
  irqstate_t flags;
  int ret;

  do
    {
      ret = nxsem_tickwait(&priv->xfer_sem, AM67_MCSPI_IRQ_TIMEOUT_TICKS);
    }
  while (ret == -EINTR);

  flags = enter_critical_section();
  am67_mcspi_putreg(priv->base, AM67_MCSPI_IRQENABLE, 0);

  if (x->active)
    {
      x->active = false;
      x->ok = false;
      priv->stats.irq_timeouts++;
      priv->stats.fail_rx = x->rx;
    }

  while (nxsem_trywait(&priv->xfer_sem) == 0)
    {
    }

  leave_critical_section(flags);

  if (x->ok)
    {
      priv->stats.irq_xfers++;
    }

  return x->ok;
}

/* Full-duplex 8-bit transfer through the FIFO in turbo mode, so the words
 * go out back to back instead of waiting for a status round trip per byte
 * (each MCU-domain register access costs ~200 ns, a byte at 6.9 MHz
 * 1.2 us). Up to one FIFO half is queued first; transfers longer than
 * that continue in the interrupt handler when the caller can sleep, else
 * by polling.
 */

static bool am67_mcspi_exchange_fifo(FAR struct am67_mcspi_dev_s *priv,
                                     FAR const uint8_t *tx8,
                                     FAR uint8_t *rx8, size_t nwords)
{
  FAR struct am67_mcspi_xfer_s *x = &priv->xfer;
  const uint32_t choff = AM67_MCSPI_CH_OFFSET(priv->channel);
  const uint32_t txreg = priv->base + AM67_MCSPI_TX0 + choff;
  const uint32_t chconf = priv->chconf;
  const bool use_irq = nwords > AM67_MCSPI_FIFO_HALF &&
                       am67_mcspi_can_sleep(priv);
  uint32_t level = (uint32_t)nwords << AM67_MCSPI_XFERLEVEL_WCNT_SHIFT;
  irqstate_t flags;
  bool ok;

  x->tx8 = tx8;
  x->rx8 = rx8;
  x->nwords = nwords;
  x->tx = 0;
  x->rx = 0;
  x->ok = false;

  bool inject = false;

#ifdef CONFIG_AM67_FAULT_INJECTION
  if (g_am67_mcspi_inject > 0)
    {
      g_am67_mcspi_inject--;
      inject = true;
    }
#endif

  if (use_irq)
    {
      level |= (AM67_MCSPI_IRQ_BLOCK - 1u) << AM67_MCSPI_XFERLEVEL_AFL_SHIFT;
    }

  /* The word count must be set before the channel is enabled, which also
   * resets the FIFO pointers. CS stays asserted: FORCE is kept.
   */

  am67_mcspi_channel_enable(priv, false);
  am67_mcspi_putreg(priv->base, AM67_MCSPI_XFERLEVEL, level);
  am67_mcspi_putreg(priv->base, AM67_MCSPI_CHCONF0 + choff,
                    chconf | AM67_MCSPI_CHCONF_FFEW |
                    AM67_MCSPI_CHCONF_FFER | AM67_MCSPI_CHCONF_TURBO);

  /* The handler must not run before the first half is queued */

  flags = enter_critical_section();

  if (use_irq)
    {
      am67_mcspi_putreg(priv->base, AM67_MCSPI_IRQSTATUS,
                        AM67_MCSPI_IRQ_ALL);
      x->active = true;

      /* Fault injection: the interrupt is never enabled (lost interrupt) */

      if (!inject)
        {
          am67_mcspi_putreg(priv->base, AM67_MCSPI_IRQENABLE,
                            AM67_MCSPI_IRQ_RX_FULL(priv->channel) |
                            AM67_MCSPI_IRQ_EOW);
        }
    }

  am67_mcspi_channel_enable(priv, true);

  /* Fault injection on the polled path: nothing is sent */

  while (x->tx < nwords && x->tx < AM67_MCSPI_FIFO_HALF &&
         !(inject && !use_irq))
    {
      am67_mcspi_fifo_push(priv, txreg);
    }

  leave_critical_section(flags);

  ok = use_irq ? am67_mcspi_fifo_wait(priv) : am67_mcspi_fifo_drain(priv);

  if (!ok)
    {
      spierr("ERROR: FIFO transfer ch%u %zu/%zu\n", priv->channel,
             x->rx, nwords);
      priv->errors++;
    }

  /* Every word was received, so the shift register is idle; confirm it
   * before CS can be released.
   */

  if (ok && !am67_mcspi_waitstat(priv->base, priv->channel,
                                 AM67_MCSPI_CHSTAT_EOT))
    {
      priv->stats.eot_timeouts++;
      priv->stats.fail_stat = getreg32(priv->base + AM67_MCSPI_CHSTAT0 +
                                       choff);
      priv->stats.fail_rx = x->rx;
      priv->errors++;
      ok = false;
    }

  /* Back to FIFO-less mode, channel enabled and CS still asserted, as the
   * word-at-a-time path expects.
   */

  am67_mcspi_channel_enable(priv, false);
  am67_mcspi_putreg(priv->base, AM67_MCSPI_CHCONF0 + choff, chconf);
  am67_mcspi_putreg(priv->base, AM67_MCSPI_XFERLEVEL, 0);
  am67_mcspi_channel_enable(priv, true);
  return ok;
}

static void am67_mcspi_exchange_words(FAR struct am67_mcspi_dev_s *priv,
                                      FAR const void *txbuffer,
                                      FAR void *rxbuffer, size_t nwords)
{
  uint32_t rd;
  size_t i;

  if (priv->nbits > 8)
    {
      FAR const uint16_t *tx16 = txbuffer;
      FAR uint16_t *rx16 = rxbuffer;

      for (i = 0; i < nwords; i++)
        {
          uint32_t wd = (tx16 != NULL) ? (uint32_t)tx16[i] : 0xffffu;

          if (!am67_mcspi_transfer_word(priv, wd, &rd))
            {
              return;
            }

          if (rx16 != NULL)
            {
              rx16[i] = (uint16_t)rd;
            }
        }
    }
  else
    {
      FAR const uint8_t *tx8 = txbuffer;
      FAR uint8_t *rx8 = rxbuffer;

      for (i = 0; i < nwords; i++)
        {
          uint32_t wd = (tx8 != NULL) ? (uint32_t)tx8[i] : 0xffu;

          if (!am67_mcspi_transfer_word(priv, wd, &rd))
            {
              return;
            }

          if (rx8 != NULL)
            {
              rx8[i] = (uint8_t)rd;
            }
        }
    }
}

static void am67_mcspi_exchange(FAR struct spi_dev_s *dev,
                                FAR const void *txbuffer,
                                FAR void *rxbuffer, size_t nwords)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  uint32_t start = (uint32_t)up_perf_gettime();
  uint32_t cycles;

  if (!priv->selected)
    {
      priv->errors++;
      return;
    }

  if (priv->nbits == 8 && nwords > 1 &&
      nwords <= AM67_MCSPI_XFERLEVEL_WCNT_MAX)
    {
      am67_mcspi_exchange_fifo(priv, txbuffer, rxbuffer, nwords);
    }
  else
    {
      am67_mcspi_exchange_words(priv, txbuffer, rxbuffer, nwords);
    }

  cycles = (uint32_t)up_perf_gettime() - start;
  priv->stats.words += nwords;
  priv->stats.xfer_cycles += cycles;

  if (cycles > priv->stats.max_xfer_cycles)
    {
      priv->stats.max_xfer_cycles = cycles;
      priv->stats.max_xfer_words = nwords;
    }
}
#endif

/* MCU_MCSPI0 answered as powered; until then no register is touched */

static bool g_spi0_powered;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void am67_spiinitialize(void)
{
  /* Nobody else powers MCU_MCSPI0 once Linux leaves it alone; the IMU and
   * the barometer are behind it.
   */

  if (am67_tisci_device_require(AM67_TISCI_DEV_MCU_MCSPI0) < 0)
    {
      spierr("MCU_MCSPI0 is not powered\n");
      return;
    }

  g_spi0_powered = true;

  /* Controller first: the chip-selects must be parked inactive before
   * the pads are handed to MCSPI.
   */

  am67_mcspi_controller_init(&g_spi0dev);
  am67_spi_pinmux_init();

#ifdef CONFIG_SPI_EXCHANGE
  if (irq_attach(AM67_MCSPI0_IRQ, am67_mcspi_interrupt, &g_spi0dev) == OK)
    {
      up_enable_irq(AM67_MCSPI0_IRQ);
      g_spi0dev.irq_ready = true;
    }
#endif
  spiinfo("MCU_MCSPI0 @ 0x%08" PRIx32 " rev=0x%08" PRIx32 "\n",
          g_spi0dev.base,
          am67_mcspi_getreg(g_spi0dev.base, AM67_MCSPI_REVISION));
}

FAR struct spi_dev_s *am67_spibus_initialize(int port)
{
  if (port != 0 || !g_spi0_powered)
    {
      return NULL;
    }

  return &g_spi0dev.spidev;
}

void am67_mcspi_board_select(FAR struct spi_dev_s *dev, uint8_t channel,
                             bool selected)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  uint32_t start = (uint32_t)up_perf_gettime();

  if (selected)
    {
      am67_mcspi_clear_other_force(priv, channel);
      am67_mcspi_select_channel(priv, channel);
      am67_mcspi_cs_force(priv, false);
      priv->selected = true;
    }
  else
    {
      am67_mcspi_cs_force(priv, true);
      priv->selected = false;

      /* A word timed out: the controller may be wedged (clock gated, left
       * in slave mode, FIFO state lost). Reset it so the next transfer
       * starts clean; this also parks every chip-select.
       */

      if (priv->errors != priv->reset_at)
        {
          am67_mcspi_controller_init(priv);
          priv->reset_at = priv->errors;
        }

      priv->stats.transfers++;
    }

  priv->stats.select_cycles += (uint32_t)up_perf_gettime() - start;
}

int am67_mcspi_take_errors(FAR struct spi_dev_s *dev)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  uint32_t errors = priv->errors;

  priv->errors = 0;
  priv->reset_at = 0;
  return errors > INT_MAX ? INT_MAX : (int)errors;
}

void am67_mcspi_stats(FAR struct spi_dev_s *dev,
                      FAR struct am67_mcspi_stats_s *stats, bool reset)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);

  *stats = priv->stats;

  if (reset)
    {
      memset(&priv->stats, 0, sizeof(priv->stats));
    }
}

#ifdef CONFIG_AM67_FAULT_INJECTION
void am67_mcspi_inject_fault(unsigned int count)
{
  g_am67_mcspi_inject = count;
}
#endif

#endif /* CONFIG_AM67_MCSPI0 */
