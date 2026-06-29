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
#include <stdint.h>
#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/spi/spi.h>

#include "am67_mcspi.h"
#include "am67_pinmux.h"
#include "arm_internal.h"

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
#define AM67_MCSPI_POLL_TIMEOUT     1000000u

/****************************************************************************
 * Private Types
 ****************************************************************************/

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

static struct am67_mcspi_dev_s g_spi0dev =
{
  .spidev =
    {
      &g_am67_spi0ops
    },
  .base       = AM67_MCSPI0_BASE,
  .lock       = NXMUTEX_INITIALIZER,
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

static bool am67_mcspi_waitstat(uint32_t base, uint8_t channel,
                                uint32_t mask)
{
  volatile uint32_t count = AM67_MCSPI_POLL_TIMEOUT;

  while ((am67_mcspi_getreg(base, AM67_MCSPI_CHSTAT0 +
                            AM67_MCSPI_CH_OFFSET(channel)) & mask) == 0)
    {
      if (--count == 0)
        {
          return false;
        }
    }

  return true;
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
  am67_mcspi_putreg(priv->base, AM67_MCSPI_CHCTRL0 +
                    AM67_MCSPI_CH_OFFSET(priv->channel), chctrl);
}

static void am67_mcspi_cs_force(FAR struct am67_mcspi_dev_s *priv,
                                bool deassert)
{
  uint32_t chconf = priv->chconf;

  if (deassert)
    {
      chconf |= AM67_MCSPI_CHCONF_FORCE;
    }
  else
    {
      chconf &= ~AM67_MCSPI_CHCONF_FORCE;
    }

  priv->chconf = chconf;
  am67_mcspi_putreg(priv->base, AM67_MCSPI_CHCONF0 +
                    AM67_MCSPI_CH_OFFSET(priv->channel), chconf);
}

static uint8_t am67_mcspi_calc_divisor(uint32_t speed_hz, uint32_t ref_clk_hz)
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
  uint32_t chctrl = 0;
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
    }
  else
    {
      div = (ref_clk + speed_hz - 1) / speed_hz;
      speed_hz = ref_clk / div;
      clkd = (div - 1) & 0x0fu;
      extclk = (div - 1) >> 4;
      chconf = AM67_MCSPI_CHCONF_CLKG;
      chctrl = (extclk << AM67_MCSPI_CHCTRL_EXTCLK_SHIFT) &
               AM67_MCSPI_CHCTRL_EXTCLK_MASK;
    }

  priv->frequency = speed_hz;

  chconf |= AM67_MCSPI_CHCONF_IS;
  chconf |= AM67_MCSPI_CHCONF_DPE1;
  chconf &= ~AM67_MCSPI_CHCONF_DPE0;
  chconf |= AM67_MCSPI_CHCONF_EPOL;
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

  priv->chconf = chconf;
  priv->chctrl = chctrl;

  am67_mcspi_putreg(priv->base, AM67_MCSPI_CHCONF0 +
                    AM67_MCSPI_CH_OFFSET(priv->channel), chconf);
  am67_mcspi_putreg(priv->base, AM67_MCSPI_CHCTRL0 +
                    AM67_MCSPI_CH_OFFSET(priv->channel), chctrl);
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
  uint32_t modulctrl;

  modulctrl = am67_mcspi_getreg(priv->base, AM67_MCSPI_MODULCTRL);
  modulctrl &= ~AM67_MCSPI_MODULCTRL_MS;
  modulctrl |= AM67_MCSPI_MODULCTRL_SINGLE;
  am67_mcspi_putreg(priv->base, AM67_MCSPI_MODULCTRL, modulctrl);

  am67_mcspi_apply_hwconfig(priv);
}

static uint32_t am67_mcspi_transfer_word(FAR struct am67_mcspi_dev_s *priv,
                                         uint32_t wd, bool last)
{
  uint8_t channel = priv->channel;

  if (!am67_mcspi_waitstat(priv->base, channel, AM67_MCSPI_CHSTAT_TXS))
    {
      return 0;
    }

  am67_mcspi_putreg(priv->base, AM67_MCSPI_TX0 +
                    AM67_MCSPI_CH_OFFSET(channel), wd);

  if (!am67_mcspi_waitstat(priv->base, channel, AM67_MCSPI_CHSTAT_RXS))
    {
      return 0;
    }

  wd = am67_mcspi_getreg(priv->base, AM67_MCSPI_RX0 +
                         AM67_MCSPI_CH_OFFSET(channel));

  if (last)
    {
      am67_mcspi_channel_enable(priv, false);
      am67_mcspi_channel_enable(priv, true);
    }

  return wd;
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

  priv->frequency = frequency;
  am67_mcspi_apply_hwconfig(priv);
  return priv->frequency;
}

static void am67_mcspi_setmode(FAR struct spi_dev_s *dev,
                               enum spi_mode_e mode)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);

  priv->mode = (uint8_t)mode;
  am67_mcspi_apply_hwconfig(priv);
}

static void am67_mcspi_setbits(FAR struct spi_dev_s *dev, int nbits)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);

  DEBUGASSERT(nbits == 8 || nbits == 16);
  priv->nbits = (uint8_t)nbits;
  am67_mcspi_apply_hwconfig(priv);
}

static uint32_t am67_mcspi_send(FAR struct spi_dev_s *dev, uint32_t wd)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);

  if (!priv->selected)
    {
      return 0;
    }

  return am67_mcspi_transfer_word(priv, wd, true);
}

#ifdef CONFIG_SPI_EXCHANGE
static void am67_mcspi_exchange(FAR struct spi_dev_s *dev,
                                FAR const void *txbuffer,
                                FAR void *rxbuffer, size_t nwords)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);
  FAR const uint8_t *tx8 = txbuffer;
  FAR uint8_t *rx8 = rxbuffer;
  size_t i;

  if (!priv->selected)
    {
      return;
    }

  for (i = 0; i < nwords; i++)
    {
      uint32_t wd = 0xff;
      uint32_t rd;
      bool last = (i + 1 == nwords);

      if (tx8 != NULL)
        {
          wd = tx8[i];
        }

      rd = am67_mcspi_transfer_word(priv, wd, last);

      if (rx8 != NULL)
        {
          rx8[i] = (uint8_t)rd;
        }
    }
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void am67_spiinitialize(void)
{
  am67_spi_pinmux_init();
  am67_mcspi_controller_init(&g_spi0dev);
  spiinfo("MCU_MCSPI0 @ 0x%08" PRIx32 " rev=0x%08" PRIx32 "\n",
          g_spi0dev.base,
          am67_mcspi_getreg(g_spi0dev.base, AM67_MCSPI_REVISION));
}

FAR struct spi_dev_s *am67_spibus_initialize(int port)
{
  if (port != 0)
    {
      return NULL;
    }

  return &g_spi0dev.spidev;
}

void am67_mcspi_board_select(FAR struct spi_dev_s *dev, uint32_t devid,
                             bool selected)
{
  FAR struct am67_mcspi_dev_s *priv = am67_mcspi_dev(dev);

  if (selected)
    {
      am67_mcspi_select_channel(priv, (uint8_t)devid);
      am67_mcspi_cs_force(priv, false);
      am67_mcspi_channel_enable(priv, true);
      priv->selected = true;
    }
  else
    {
      am67_mcspi_channel_enable(priv, false);
      am67_mcspi_cs_force(priv, true);
      priv->selected = false;
    }
}

#endif /* CONFIG_AM67_MCSPI0 */
