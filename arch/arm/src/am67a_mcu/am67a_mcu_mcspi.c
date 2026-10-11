/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_mcspi.c
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
#include <stdint.h>
#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/spi/spi.h>

#include "am67a_mcu_mcspi.h"
#include "am67a_mcu_pinmux.h"
#include "arm_internal.h"

#if defined(CONFIG_AM67A_MCU_MCSPI0) || defined(CONFIG_AM67A_MCU_MCSPI1)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AM67A_MCU_MCSPI_MAX_DIVIDER      (4096u)
#define AM67A_MCU_MCSPI_POLL_TIMEOUT     (1000000u)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct am67a_mcu_mcspi_dev_s
{
  struct spi_dev_s spidev;
  uint32_t         base;
  mutex_t          lock;
  uint8_t          channel;
  uint32_t         frequency[AM67A_MCU_MCSPI_MAX_CHANNELS];
  uint8_t          mode[AM67A_MCU_MCSPI_MAX_CHANNELS];
  uint8_t          nbits[AM67A_MCU_MCSPI_MAX_CHANNELS];
  uint32_t         chconf[AM67A_MCU_MCSPI_MAX_CHANNELS];
  uint32_t         chctrl[AM67A_MCU_MCSPI_MAX_CHANNELS];
  bool             selected;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int       am67a_mcu_mcspi_lock(FAR struct spi_dev_s *dev, bool lock);
static uint32_t  am67a_mcu_mcspi_setfrequency(FAR struct spi_dev_s *dev,
                                          uint32_t frequency);
static void      am67a_mcu_mcspi_setmode(FAR struct spi_dev_s *dev,
                                     enum spi_mode_e mode);
static void      am67a_mcu_mcspi_setbits(FAR struct spi_dev_s *dev, int nbits);
static uint32_t  am67a_mcu_mcspi_send(FAR struct spi_dev_s *dev, uint32_t wd);
#ifdef CONFIG_SPI_EXCHANGE
static void      am67a_mcu_mcspi_exchange(FAR struct spi_dev_s *dev,
                                      FAR const void *txbuffer,
                                      FAR void *rxbuffer, size_t nwords);
#endif

/* Default board-level CS and status hooks */

void am67a_mcu_spi0select(FAR struct spi_dev_s *dev, uint32_t devid,
                      bool selected) weak_function;
uint8_t am67a_mcu_spi0status(FAR struct spi_dev_s *dev,
                         uint32_t devid) weak_function;

#ifdef CONFIG_AM67A_MCU_MCSPI1
void am67a_mcu_spi1select(FAR struct spi_dev_s *dev, uint32_t devid,
                      bool selected) weak_function;
uint8_t am67a_mcu_spi1status(FAR struct spi_dev_s *dev,
                         uint32_t devid) weak_function;
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_AM67A_MCU_MCSPI0
static const struct spi_ops_s g_am67a_mcu_spi0ops =
{
  .lock         = am67a_mcu_mcspi_lock,
  .select       = am67a_mcu_spi0select,
  .setfrequency = am67a_mcu_mcspi_setfrequency,
  .setmode      = am67a_mcu_mcspi_setmode,
  .setbits      = am67a_mcu_mcspi_setbits,
  .status       = am67a_mcu_spi0status,
  .send         = am67a_mcu_mcspi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange     = am67a_mcu_mcspi_exchange,
#endif
};

static struct am67a_mcu_mcspi_dev_s g_spi0dev =
{
  .spidev =
    {
      &g_am67a_mcu_spi0ops
    },
  .base       = AM67A_MCU_MCU_MCSPI0_BASE,
  .lock       = NXMUTEX_INITIALIZER,
  .channel    = 0,
  .frequency  = { 1000000, 1000000, 1000000, 1000000 },
  .mode       = { SPIDEV_MODE3, SPIDEV_MODE3, SPIDEV_MODE3, SPIDEV_MODE3 },
  .nbits      = { 8, 8, 8, 8 },
};
#endif

#ifdef CONFIG_AM67A_MCU_MCSPI1
static const struct spi_ops_s g_am67a_mcu_spi1ops =
{
  .lock         = am67a_mcu_mcspi_lock,
  .select       = am67a_mcu_spi1select,
  .setfrequency = am67a_mcu_mcspi_setfrequency,
  .setmode      = am67a_mcu_mcspi_setmode,
  .setbits      = am67a_mcu_mcspi_setbits,
  .status       = am67a_mcu_spi1status,
  .send         = am67a_mcu_mcspi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange     = am67a_mcu_mcspi_exchange,
#endif
};

static struct am67a_mcu_mcspi_dev_s g_spi1dev =
{
  .spidev =
    {
      &g_am67a_mcu_spi1ops
    },
  .base       = AM67A_MCU_MCU_MCSPI1_BASE,
  .lock       = NXMUTEX_INITIALIZER,
  .channel    = 0,
  .frequency  = { 1000000, 1000000, 1000000, 1000000 },
  .mode       = { SPIDEV_MODE3, SPIDEV_MODE3, SPIDEV_MODE3, SPIDEV_MODE3 },
  .nbits      = { 8, 8, 8, 8 },
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t am67a_mcu_mcspi_getreg(uint32_t base, uint32_t offset)
{
  return getreg32(base + offset);
}

static inline void am67a_mcu_mcspi_putreg(uint32_t base, uint32_t offset,
                                      uint32_t value)
{
  putreg32(value, base + offset);
}

static inline FAR struct am67a_mcu_mcspi_dev_s *
am67a_mcu_mcspi_dev(FAR struct spi_dev_s *dev)
{
  return (FAR struct am67a_mcu_mcspi_dev_s *)dev;
}

static bool am67a_mcu_mcspi_waitstat(uint32_t base, uint8_t channel,
                                     uint32_t mask, FAR const char *tag)
{
  uint32_t count = AM67A_MCU_MCSPI_POLL_TIMEOUT;
  uint32_t stat;

  while (((stat = am67a_mcu_mcspi_getreg(base, AM67A_MCU_MCSPI_CHSTAT0 +
                             AM67A_MCU_MCSPI_CH_OFFSET(channel))) & mask) == 0)
    {
      if (--count == 0)
        {
          uint32_t chconf = am67a_mcu_mcspi_getreg(base, AM67A_MCU_MCSPI_CHCONF0 +
                                                    AM67A_MCU_MCSPI_CH_OFFSET(channel));
          uint32_t chctrl = am67a_mcu_mcspi_getreg(base, AM67A_MCU_MCSPI_CHCTRL0 +
                                                    AM67A_MCU_MCSPI_CH_OFFSET(channel));
          spierr("[MCSPI TIMEOUT] ch%u waiting for %s (mask=0x%08" PRIx32
                 ", STAT=0x%08" PRIx32 ", CONF=0x%08" PRIx32
                 ", CTRL=0x%08" PRIx32 ")\n",
                 channel, tag, (uint32_t)mask, stat, chconf, chctrl);
          return false;
        }
    }

  return true;
}

static void am67a_mcu_mcspi_channel_enable(FAR struct am67a_mcu_mcspi_dev_s *priv,
                                       bool enable)
{
  uint8_t ch = priv->channel;
  uint32_t chctrl = priv->chctrl[ch];

  if (enable)
    {
      chctrl |= AM67A_MCU_MCSPI_CHCTRL_EN;
    }
  else
    {
      chctrl &= ~AM67A_MCU_MCSPI_CHCTRL_EN;
    }

  priv->chctrl[ch] = chctrl;
  am67a_mcu_mcspi_putreg(priv->base,
                     AM67A_MCU_MCSPI_CHCTRL0 +
                     AM67A_MCU_MCSPI_CH_OFFSET(ch),
                     chctrl);
}

static void am67a_mcu_mcspi_cs_force(FAR struct am67a_mcu_mcspi_dev_s *priv,
                                 bool deassert)
{
  uint8_t ch = priv->channel;
  uint32_t chconf = priv->chconf[ch];

  if (deassert)
    {
      if (priv->selected)
        {
          /* Wait for physical transmission to complete on the wire before releasing CS */

          am67a_mcu_mcspi_waitstat(priv->base, ch,
                                   AM67A_MCU_MCSPI_CHSTAT_EOT, "EOT");
        }

      chconf &= ~AM67A_MCU_MCSPI_CHCONF_FORCE;  /* FORCE=0: CS deasserted (high) */
    }
  else
    {
      chconf |= AM67A_MCU_MCSPI_CHCONF_FORCE;   /* FORCE=1: CS asserted (low, EPOL=1) */
    }

  priv->chconf[ch] = chconf;
  am67a_mcu_mcspi_putreg(priv->base,
                     AM67A_MCU_MCSPI_CHCONF0 +
                     AM67A_MCU_MCSPI_CH_OFFSET(ch),
                     chconf);

  /* Enable channel on assert, disable on deassert per TRM standard */

  am67a_mcu_mcspi_channel_enable(priv, !deassert);
}

static uint8_t am67a_mcu_mcspi_calc_divisor(uint32_t speed_hz,
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

static void am67a_mcu_mcspi_apply_hwconfig(FAR struct am67a_mcu_mcspi_dev_s *priv)
{
  uint8_t ch = priv->channel;
  uint32_t ref_clk = CONFIG_AM67A_MCU_MCSPI_FCLK;
  uint32_t speed_hz = priv->frequency[ch];
  uint32_t chconf;
  uint32_t chctrl = priv->chctrl[ch] & AM67A_MCU_MCSPI_CHCTRL_EN; /* preserve EN bit */
  uint8_t clkd;
  uint32_t div;
  uint32_t extclk = 0;

  if (speed_hz > ref_clk)
    {
      speed_hz = ref_clk;
    }

  if (speed_hz < (ref_clk / AM67A_MCU_MCSPI_MAX_DIVIDER))
    {
      clkd = am67a_mcu_mcspi_calc_divisor(speed_hz, ref_clk);
      speed_hz = ref_clk >> (clkd + 1);
      chconf = 0;
      chctrl &= ~AM67A_MCU_MCSPI_CHCTRL_EXTCLK_MASK;
    }
  else
    {
      div = (ref_clk + speed_hz - 1) / speed_hz;
      speed_hz = ref_clk / div;
      clkd = (div - 1) & 0x0fu;
      extclk = (div - 1) >> 4;
      chconf = AM67A_MCU_MCSPI_CHCONF_CLKG;
      chctrl &= ~AM67A_MCU_MCSPI_CHCTRL_EXTCLK_MASK;
      chctrl |= (extclk << AM67A_MCU_MCSPI_CHCTRL_EXTCLK_SHIFT) &
                AM67A_MCU_MCSPI_CHCTRL_EXTCLK_MASK;
    }

  priv->frequency[ch] = speed_hz;

  chconf |= AM67A_MCU_MCSPI_CHCONF_IS;    /* IS=1: receive from D1 (MISO) */
  chconf &= ~AM67A_MCU_MCSPI_CHCONF_DPE0; /* DPE0=0: TX enabled on D0 (MOSI) */
  chconf |= AM67A_MCU_MCSPI_CHCONF_DPE1;  /* DPE1=1: TX disabled on D1 */
  chconf |= AM67A_MCU_MCSPI_CHCONF_EPOL;  /* EPOL=1: CS active-low */
  chconf |= AM67A_MCU_MCSPI_CHCONF_TCS_2P5CYCLES; /* TCS=3: 2.5 cycles CS time control */
  chconf &= ~AM67A_MCU_MCSPI_CHCONF_WL_MASK;
  chconf |= ((uint32_t)(priv->nbits[ch] - 1) << AM67A_MCU_MCSPI_CHCONF_WL_SHIFT);
  chconf &= ~AM67A_MCU_MCSPI_CHCONF_CLKD_MASK;
  chconf |= ((uint32_t)clkd << AM67A_MCU_MCSPI_CHCONF_CLKD_SHIFT);

  if ((priv->mode[ch] & SPIDEV_MODE2) != 0)
    {
      chconf |= AM67A_MCU_MCSPI_CHCONF_POL;
    }
  else
    {
      chconf &= ~AM67A_MCU_MCSPI_CHCONF_POL;
    }

  if ((priv->mode[ch] & SPIDEV_MODE1) != 0)
    {
      chconf |= AM67A_MCU_MCSPI_CHCONF_PHA;
    }
  else
    {
      chconf &= ~AM67A_MCU_MCSPI_CHCONF_PHA;
    }

  /* SPIENSLV: route this channel to its natural CS pin (chN uses CSN) */

  chconf &= ~AM67A_MCU_MCSPI_CHCONF_SPIENSLV_MASK;
  chconf |= ((uint32_t)ch << AM67A_MCU_MCSPI_CHCONF_SPIENSLV_SHIFT) &
             AM67A_MCU_MCSPI_CHCONF_SPIENSLV_MASK;

  /* Re-assert FORCE if the channel is currently selected so that
   * reconfiguring mid-transaction does not glitch the CS line.
   */

  if (priv->selected)
    {
      chconf |= AM67A_MCU_MCSPI_CHCONF_FORCE;
    }

  priv->chconf[ch] = chconf;
  priv->chctrl[ch] = chctrl;

  am67a_mcu_mcspi_putreg(priv->base,
                     AM67A_MCU_MCSPI_CHCONF0 +
                     AM67A_MCU_MCSPI_CH_OFFSET(ch),
                     chconf);
  am67a_mcu_mcspi_putreg(priv->base,
                     AM67A_MCU_MCSPI_CHCTRL0 +
                     AM67A_MCU_MCSPI_CH_OFFSET(ch),
                     chctrl);

  if (priv->selected)
    {
      am67a_mcu_mcspi_channel_enable(priv, true);
    }
}

static void am67a_mcu_mcspi_select_channel(FAR struct am67a_mcu_mcspi_dev_s *priv,
                                       uint8_t channel)
{
  if (channel >= AM67A_MCU_MCSPI_MAX_CHANNELS)
    {
      return;
    }

  if (priv->channel == channel)
    {
      return;
    }

  am67a_mcu_mcspi_channel_enable(priv, false);
  priv->channel = channel;
  am67a_mcu_mcspi_apply_hwconfig(priv);
}

static void am67a_mcu_mcspi_controller_init(FAR struct am67a_mcu_mcspi_dev_s *priv)
{
  uint32_t regval;
  uint32_t modulctrl;

  /* Configure interconnect to no-idle so OCP does not gate clock while polling */

  am67a_mcu_mcspi_putreg(priv->base, AM67A_MCU_MCSPI_HL_SYSCONFIG,
                     AM67A_MCU_MCSPI_HL_SYSCONFIG_NOIDLE);

  /* Trigger software reset */

  regval = am67a_mcu_mcspi_getreg(priv->base, AM67A_MCU_MCSPI_SYSCONFIG);
  regval |= AM67A_MCU_MCSPI_SYSCONFIG_SOFTRESET;
  am67a_mcu_mcspi_putreg(priv->base, AM67A_MCU_MCSPI_SYSCONFIG, regval);

  /* Wait for SYSSTATUS.RESETDONE */

  uint32_t rst_timeout = AM67A_MCU_MCSPI_POLL_TIMEOUT;
  do
    {
      regval = am67a_mcu_mcspi_getreg(priv->base, AM67A_MCU_MCSPI_SYSSTATUS);
      if (--rst_timeout == 0)
        {
          spierr("ERROR: MCSPI controller reset timeout\n");
          break;
        }
    }
  while ((regval & AM67A_MCU_MCSPI_SYSSTATUS_RESETDONE) == 0);

  /* Set Clock Activity and Idle Mode */

  regval = AM67A_MCU_MCSPI_SYSCONFIG_CLKACT_BOTH |
           AM67A_MCU_MCSPI_SYSCONFIG_SIDLEMODE_NO;
  am67a_mcu_mcspi_putreg(priv->base, AM67A_MCU_MCSPI_SYSCONFIG, regval);

  /* Initialize all 4 channels with default active-low EPOL configuration */

  for (int ch = 0; ch < AM67A_MCU_MCSPI_MAX_CHANNELS; ch++)
    {
      priv->channel = ch;
      am67a_mcu_mcspi_apply_hwconfig(priv);
      am67a_mcu_mcspi_channel_enable(priv, false);
    }

  priv->channel = 0;

  /* SINGLE=1: Single channel active at a time; manual CS via FORCE */

  modulctrl = AM67A_MCU_MCSPI_MODULCTRL_SINGLE;
  am67a_mcu_mcspi_putreg(priv->base, AM67A_MCU_MCSPI_MODULCTRL, modulctrl);
}

static uint32_t am67a_mcu_mcspi_transfer_word(FAR struct am67a_mcu_mcspi_dev_s *priv,
                                          uint32_t wd)
{
  uint32_t choff = AM67A_MCU_MCSPI_CH_OFFSET(priv->channel);

  if (!am67a_mcu_mcspi_waitstat(priv->base, priv->channel,
                                AM67A_MCU_MCSPI_CHSTAT_TXS, "TXS"))
    {
      spierr("ERROR: MCSPI TX timeout ch%u\n", priv->channel);
      return 0;
    }

  am67a_mcu_mcspi_putreg(priv->base, AM67A_MCU_MCSPI_TX0 + choff, wd);

  if (!am67a_mcu_mcspi_waitstat(priv->base, priv->channel,
                                AM67A_MCU_MCSPI_CHSTAT_RXS, "RXS"))
    {
      spierr("ERROR: MCSPI RX timeout ch%u\n", priv->channel);
      return 0;
    }

  return am67a_mcu_mcspi_getreg(priv->base, AM67A_MCU_MCSPI_RX0 + choff);
}

/****************************************************************************
 * SPI Driver Methods
 ****************************************************************************/

static int am67a_mcu_mcspi_lock(FAR struct spi_dev_s *dev, bool lock)
{
  FAR struct am67a_mcu_mcspi_dev_s *priv = am67a_mcu_mcspi_dev(dev);

  if (lock)
    {
      return nxmutex_lock(&priv->lock);
    }

  nxmutex_unlock(&priv->lock);
  return OK;
}

static uint32_t am67a_mcu_mcspi_setfrequency(FAR struct spi_dev_s *dev,
                                         uint32_t frequency)
{
  FAR struct am67a_mcu_mcspi_dev_s *priv = am67a_mcu_mcspi_dev(dev);
  uint8_t ch = priv->channel;

  if (priv->frequency[ch] == frequency)
    {
      return priv->frequency[ch];
    }

  priv->frequency[ch] = frequency;
  am67a_mcu_mcspi_apply_hwconfig(priv);
  return priv->frequency[ch];
}

static void am67a_mcu_mcspi_setmode(FAR struct spi_dev_s *dev,
                                enum spi_mode_e mode)
{
  FAR struct am67a_mcu_mcspi_dev_s *priv = am67a_mcu_mcspi_dev(dev);
  uint8_t ch = priv->channel;

  if (priv->mode[ch] == (uint8_t)mode)
    {
      return;
    }

  priv->mode[ch] = (uint8_t)mode;
  am67a_mcu_mcspi_apply_hwconfig(priv);
}

static void am67a_mcu_mcspi_setbits(FAR struct spi_dev_s *dev, int nbits)
{
  FAR struct am67a_mcu_mcspi_dev_s *priv = am67a_mcu_mcspi_dev(dev);
  uint8_t ch = priv->channel;

  DEBUGASSERT(nbits == 8 || nbits == 16);

  if (priv->nbits[ch] == (uint8_t)nbits)
    {
      return;
    }

  priv->nbits[ch] = (uint8_t)nbits;
  am67a_mcu_mcspi_apply_hwconfig(priv);
}

static uint32_t am67a_mcu_mcspi_send(FAR struct spi_dev_s *dev, uint32_t wd)
{
  FAR struct am67a_mcu_mcspi_dev_s *priv = am67a_mcu_mcspi_dev(dev);

  if (!priv->selected)
    {
      return 0;
    }

  return am67a_mcu_mcspi_transfer_word(priv, wd);
}

#ifdef CONFIG_SPI_EXCHANGE
static void am67a_mcu_mcspi_exchange(FAR struct spi_dev_s *dev,
                                 FAR const void *txbuffer,
                                 FAR void *rxbuffer, size_t nwords)
{
  FAR struct am67a_mcu_mcspi_dev_s *priv = am67a_mcu_mcspi_dev(dev);
  size_t i;

  if (!priv->selected)
    {
      return;
    }

  if (priv->nbits[priv->channel] > 8)
    {
      FAR const uint16_t *tx16 = txbuffer;
      FAR uint16_t *rx16 = rxbuffer;

      for (i = 0; i < nwords; i++)
        {
          uint32_t wd = (tx16 != NULL) ? (uint32_t)tx16[i] : 0xffffu;
          uint32_t rd = am67a_mcu_mcspi_transfer_word(priv, wd);

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
          uint32_t rd = am67a_mcu_mcspi_transfer_word(priv, wd);

          if (rx8 != NULL)
            {
              rx8[i] = (uint8_t)rd;
            }
        }
    }
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static bool g_spi0_initialized = false;
#ifdef CONFIG_AM67A_MCU_MCSPI1
static bool g_spi1_initialized = false;
#endif

void am67a_mcu_spiinitialize(void)
{
#ifdef CONFIG_AM67A_MCU_MCSPI0
  if (!g_spi0_initialized)
    {
      am67a_mcu_spi0_pinmux_init();
      am67a_mcu_mcspi_controller_init(&g_spi0dev);
      g_spi0_initialized = true;
      spiinfo("MCU_MCSPI0 @ 0x%08" PRIx32 " rev=0x%08" PRIx32 "\n",
              g_spi0dev.base,
              am67a_mcu_mcspi_getreg(g_spi0dev.base, AM67A_MCU_MCSPI_REVISION));
    }
#endif

#ifdef CONFIG_AM67A_MCU_MCSPI1
  if (!g_spi1_initialized)
    {
      am67a_mcu_mcspi_controller_init(&g_spi1dev);
      g_spi1_initialized = true;
      spiinfo("MCU_MCSPI1 @ 0x%08" PRIx32 " rev=0x%08" PRIx32 "\n",
              g_spi1dev.base,
              am67a_mcu_mcspi_getreg(g_spi1dev.base, AM67A_MCU_MCSPI_REVISION));
    }
#endif
}

FAR struct spi_dev_s *am67a_mcu_spibus_initialize(int port)
{
#ifdef CONFIG_AM67A_MCU_MCSPI0
  if (port == 0)
    {
      am67a_mcu_spiinitialize();
      return &g_spi0dev.spidev;
    }
#endif

#ifdef CONFIG_AM67A_MCU_MCSPI1
  if (port == 1)
    {
      am67a_mcu_spiinitialize();
      return &g_spi1dev.spidev;
    }
#endif

  return NULL;
}

void am67a_mcu_mcspi_board_select(FAR struct spi_dev_s *dev, uint8_t channel,
                              bool selected)
{
  FAR struct am67a_mcu_mcspi_dev_s *priv = am67a_mcu_mcspi_dev(dev);

  if (selected)
    {
      am67a_mcu_mcspi_select_channel(priv, channel);
      am67a_mcu_mcspi_cs_force(priv, false);
      priv->selected = true;
    }
  else
    {
      am67a_mcu_mcspi_cs_force(priv, true);
      priv->selected = false;
    }
}

/* Default board select & status implementations */

void am67a_mcu_spi0select(FAR struct spi_dev_s *dev, uint32_t devid,
                      bool selected)
{
  uint8_t channel;

  switch (SPIDEVID_TYPE(devid))
    {
      case SPIDEVTYPE_IMU:
        /* Board routing: ICM-20948 IMU is hardwired to MCU_SPI0 CS3 */

        channel = 3;
        break;

      case SPIDEVTYPE_BAROMETER:
        /* Board routing: BMP390 Barometer is hardwired to MCU_SPI0 CS1 */

        channel = 1;
        break;

      default:
        /* Direct channel index or SPIDEVID_INDEX */

        channel = (uint8_t)(SPIDEVID_INDEX(devid) & 0x03);
        break;
    }

  am67a_mcu_mcspi_board_select(dev, channel, selected);
}

uint8_t am67a_mcu_spi0status(FAR struct spi_dev_s *dev, uint32_t devid)
{
  return SPI_STATUS_PRESENT;
}

#ifdef CONFIG_AM67A_MCU_MCSPI1
void am67a_mcu_spi1select(FAR struct spi_dev_s *dev, uint32_t devid,
                      bool selected)
{
  uint8_t channel = (uint8_t)(SPIDEVID_INDEX(devid) & 0x03);
  am67a_mcu_mcspi_board_select(dev, channel, selected);
}

uint8_t am67a_mcu_spi1status(FAR struct spi_dev_s *dev, uint32_t devid)
{
  return SPI_STATUS_PRESENT;
}
#endif

#endif /* CONFIG_AM67A_MCU_MCSPI0 || CONFIG_AM67A_MCU_MCSPI1 */
