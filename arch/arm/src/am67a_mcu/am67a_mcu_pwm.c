/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_pwm.c
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
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/timers/pwm.h>
#include <arch/chip/am67a_mcu_pwm.h>

#include "arm_internal.h"
#include "am67a_mcu_pinmux.h"
#include "am67a_mcu_pwm.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* TBCTL Bitfields */

#define TBCTL_CTRMODE_UP           0x0000u
#define TBCTL_CTRMODE_DOWN         0x0001u
#define TBCTL_CTRMODE_UPDOWN       0x0002u
#define TBCTL_CTRMODE_FREEZE       0x0003u
#define TBCTL_CTRMODE_MASK         0x0003u

#define TBCTL_PRDLD_SHDW           0x0000u
#define TBCTL_PRDLD_IMDT           0x0008u
#define TBCTL_PRDLD_MASK           0x0008u

#define TBCTL_HSPCLKDIV_SHIFT      7
#define TBCTL_HSPCLKDIV_MASK       (0x7u << 7)

#define TBCTL_CLKDIV_SHIFT         10
#define TBCTL_CLKDIV_MASK          (0x7u << 10)

/* Action Qualifier Control (AQCTLA, AQCTLB) */

#define AQCTL_ZRO_SETHIGH          0x0002u
#define AQCTL_CAU_CLEARLOW         0x0010u
#define AQCTL_CBU_CLEARLOW         0x0100u

/* Continuous Software Force (AQCSFRC) */

#define AQCSFRC_CSFA_DIS           0x0000u
#define AQCSFRC_CSFA_LOW           0x0001u
#define AQCSFRC_CSFA_HIGH          0x0002u
#define AQCSFRC_CSFA_MASK          0x0003u

#define AQCSFRC_CSFB_DIS           0x0000u
#define AQCSFRC_CSFB_LOW           0x0004u
#define AQCSFRC_CSFB_HIGH          0x0008u
#define AQCSFRC_CSFB_MASK          0x000cu

/* Software Force Register (AQSFRC) */

#define AQSFRC_RLDCSF_IMDT         (0x3u << 6)
#define AQSFRC_RLDCSF_ZRO          (0x0u << 6)
#define AQSFRC_RLDCSF_MASK         (0x3u << 6)

#define NUM_EPWM_MODULES           2
#define NUM_EPWM_CHANNELS          4
#define NUM_ECAP_MODULES           3
#define DEFAULT_PWM_FREQ           400 /* 400 Hz Drone ESC default */

/* ECAP Control Register 2 Bitfields */

#define ECCTL2_TSCTR_FREERUN       0x0010u
#define ECCTL2_SYNC_SEL_DISA       0x00c0u
#define ECCTL2_APWM_MODE           0x0200u

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct am67a_epwm_mod_s
{
  uintptr_t base;
  uint32_t  freq_hz;
  uint16_t  ps_div;
  uint16_t  tbprd;
  bool      initialized;
};

struct am67a_epwm_chan_s
{
  int      mod_idx;  /* 0 for EPWM0, 1 for EPWM1 */
  int      subchan;  /* 0 for A, 1 for B */
  uint32_t pulse_us;
  bool     enabled;
};

struct am67a_ecap_chan_s
{
  uintptr_t base;
  uint32_t  freq_hz;
  uint32_t  pulse_us;
  bool      enabled;
};

#ifdef CONFIG_PWM
struct am67a_pwm_dev_s
{
  struct pwm_lowerhalf_s lower;
  int                    mod_idx;
};
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct am67a_epwm_mod_s g_epwm_mods[NUM_EPWM_MODULES] =
{
  {
    .base        = AM67A_MCU_EPWM0_BASE,
    .freq_hz     = DEFAULT_PWM_FREQ,
    .ps_div      = 1,
    .tbprd       = 0,
    .initialized = false,
  },
  {
    .base        = AM67A_MCU_EPWM1_BASE,
    .freq_hz     = DEFAULT_PWM_FREQ,
    .ps_div      = 1,
    .tbprd       = 0,
    .initialized = false,
  }
};

static struct am67a_epwm_chan_s g_epwm_chans[NUM_EPWM_CHANNELS] =
{
  { .mod_idx = 0, .subchan = 0, .pulse_us = 0, .enabled = false }, /* CH0: EPWM0_A, Pin 29 */
  { .mod_idx = 0, .subchan = 1, .pulse_us = 0, .enabled = false }, /* CH1: EPWM0_B, Pin 8  */
  { .mod_idx = 1, .subchan = 0, .pulse_us = 0, .enabled = false }, /* CH2: EPWM1_A, Pin 31 */
  { .mod_idx = 1, .subchan = 1, .pulse_us = 0, .enabled = false }, /* CH3: EPWM1_B, Pin 33 */
};

static struct am67a_ecap_chan_s g_ecap_chans[NUM_ECAP_MODULES] =
{
  { .base = AM67A_MCU_ECAP0_BASE, .freq_hz = DEFAULT_PWM_FREQ, .pulse_us = 0, .enabled = false }, /* CH4: Hat Pin 32 */
  { .base = AM67A_MCU_ECAP1_BASE, .freq_hz = DEFAULT_PWM_FREQ, .pulse_us = 0, .enabled = false }, /* CH5: Hat Pin 36 */
  { .base = AM67A_MCU_ECAP2_BASE, .freq_hz = DEFAULT_PWM_FREQ, .pulse_us = 0, .enabled = false }, /* CH6: Hat Pin 12 */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint16_t epwm_read16(uintptr_t base, uint32_t offset)
{
  return getreg16(base + offset);
}

static inline void epwm_write16(uintptr_t base, uint32_t offset, uint16_t val)
{
  putreg16(val, base + offset);
}

static inline void epwm_modify16(uintptr_t base, uint32_t offset,
                                 uint16_t mask, uint16_t val)
{
  uint16_t r = getreg16(base + offset);
  r &= ~mask;
  r |= (val & mask);
  putreg16(r, base + offset);
}

/****************************************************************************
 * Name: epwm_calc_prescale
 *
 * Description:
 *   Computes optimal CLKDIV and HSPCLKDIV for a target period in cycles.
 ****************************************************************************/

static int epwm_calc_prescale(uint32_t period_cycles,
                              uint16_t *ps_div, uint16_t *tb_clk_div)
{
  uint32_t clkdiv;
  uint32_t hspclkdiv;
  uint32_t rqst_prescaler = period_cycles / 65535u;

  for (clkdiv = 0; clkdiv <= 7; clkdiv++)
    {
      for (hspclkdiv = 0; hspclkdiv <= 7; hspclkdiv++)
        {
          uint32_t div = (1u << clkdiv) * (hspclkdiv ? (hspclkdiv * 2u) : 1u);
          if (div > rqst_prescaler)
            {
              *ps_div = (uint16_t)div;
              *tb_clk_div = (uint16_t)((clkdiv << TBCTL_CLKDIV_SHIFT) |
                                       (hspclkdiv << TBCTL_HSPCLKDIV_SHIFT));
              return OK;
            }
        }
    }

  return -ERANGE;
}

/****************************************************************************
 * Name: epwm_configure_module
 *
 * Description:
 *   Configure frequency, prescalers, and period register for an ePWM module.
 ****************************************************************************/

static int epwm_configure_module(int mod_idx, uint32_t freq_hz)
{
  struct am67a_epwm_mod_s *mod = &g_epwm_mods[mod_idx];
  uint32_t period_cycles;
  uint16_t ps_div = 1;
  uint16_t tb_clk_div = 0;
  int ret;

  if (freq_hz == 0 || freq_hz > 500000u)
    {
      return -EINVAL;
    }

  period_cycles = AM67A_MCU_EPWM_CLK_FREQ / freq_hz;

  ret = epwm_calc_prescale(period_cycles, &ps_div, &tb_clk_div);
  if (ret < 0)
    {
      return ret;
    }

  mod->freq_hz = freq_hz;
  mod->ps_div  = ps_div;
  mod->tbprd   = (uint16_t)(period_cycles / ps_div);

  /* Configure TBCTL: Up-count mode, shadow load period, clock dividers */

  uint16_t tbctl = TBCTL_CTRMODE_UP | TBCTL_PRDLD_SHDW | tb_clk_div;
  epwm_write16(mod->base, AM67A_EPWM_TBCTL_OFFSET, tbctl);

  /* Set period register */

  epwm_write16(mod->base, AM67A_EPWM_TBPRD_OFFSET, mod->tbprd);

  /* Reset counter */

  epwm_write16(mod->base, AM67A_EPWM_TBCNT_OFFSET, 0);

  return OK;
}

/****************************************************************************
 * Name: ecap_configure
 ****************************************************************************/

static int ecap_configure(int idx, uint32_t freq_hz)
{
  if (idx < 0 || idx >= NUM_ECAP_MODULES || freq_hz == 0 || freq_hz > 1000000u)
    {
      return -EINVAL;
    }

  uintptr_t base = g_ecap_chans[idx].base;
  uint32_t period_cycles = AM67A_MCU_ECAP_CLK_FREQ / freq_hz;
  g_ecap_chans[idx].freq_hz = freq_hz;

  /* Configure APWM mode: TSCTR_FREERUN | APWM_MODE | SYNC_SEL_DISA */

  uint16_t ctl2 = getreg16(base + AM67A_ECAP_ECCTL2_OFFSET);
  ctl2 |= ECCTL2_TSCTR_FREERUN | ECCTL2_APWM_MODE | ECCTL2_SYNC_SEL_DISA;
  putreg16(ctl2, base + AM67A_ECAP_ECCTL2_OFFSET);

  /* Write period to CAP1 (active) and CAP3 (shadow) */

  putreg32(period_cycles, base + AM67A_ECAP_CAP1_OFFSET);
  putreg32(period_cycles, base + AM67A_ECAP_CAP3_OFFSET);

  /* Reset counter */

  putreg32(0, base + AM67A_ECAP_TSCTR_OFFSET);

  return OK;
}

/****************************************************************************
 * Name: ecap_set_duty_us
 ****************************************************************************/

static int ecap_set_duty_us(int idx, uint32_t pulse_us)
{
  if (idx < 0 || idx >= NUM_ECAP_MODULES)
    {
      return -EINVAL;
    }

  uintptr_t base = g_ecap_chans[idx].base;
  g_ecap_chans[idx].pulse_us = pulse_us;

  if (pulse_us == 0)
    {
      /* Active LOW: In Active-High mode (APWMPOL=0), setting compare
       * (CAP2/CAP4) to 0 forces the output continuously LOW (0V / 0% duty).
       */

      putreg32(0, base + AM67A_ECAP_CAP2_OFFSET);
      putreg32(0, base + AM67A_ECAP_CAP4_OFFSET);

      uint16_t ctl2 = getreg16(base + AM67A_ECAP_ECCTL2_OFFSET);
      ctl2 |= ECCTL2_TSCTR_FREERUN | ECCTL2_APWM_MODE | ECCTL2_SYNC_SEL_DISA;
      putreg16(ctl2, base + AM67A_ECAP_ECCTL2_OFFSET);

      g_ecap_chans[idx].enabled = false;
      return OK;
    }

  uint32_t period_cycles = AM67A_MCU_ECAP_CLK_FREQ / g_ecap_chans[idx].freq_hz;
  uint64_t duty_cycles = ((uint64_t)AM67A_MCU_ECAP_CLK_FREQ * pulse_us) / 1000000ull;
  if (duty_cycles > period_cycles)
    {
      duty_cycles = period_cycles;
    }

  /* Write compare to CAP2 (active) and CAP4 (shadow) */

  putreg32((uint32_t)duty_cycles, base + AM67A_ECAP_CAP2_OFFSET);
  putreg32((uint32_t)duty_cycles, base + AM67A_ECAP_CAP4_OFFSET);

  /* Enable APWM output */

  uint16_t ctl2 = getreg16(base + AM67A_ECAP_ECCTL2_OFFSET);
  ctl2 |= ECCTL2_TSCTR_FREERUN | ECCTL2_APWM_MODE | ECCTL2_SYNC_SEL_DISA;
  putreg16(ctl2, base + AM67A_ECAP_ECCTL2_OFFSET);

  g_ecap_chans[idx].enabled = true;
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_pwm_init
 ****************************************************************************/

int am67a_mcu_pwm_init(void)
{
  static bool s_initialized = false;
  int m;
  int e;
  int ret;

  if (s_initialized)
    {
      return OK;
    }

  /* 1. Setup pin multiplexing on expansion header (EPWM + ECAP) */

  am67a_mcu_pwm_pinmux_init();

  /* 2. Enable Time-Base clock in Main CTRL MMR */

  am67a_mcu_pwm_tbclk_enable();

  /* 3. Initialize EPWM0 and EPWM1 */

  for (m = 0; m < NUM_EPWM_MODULES; m++)
    {
      uintptr_t base = g_epwm_mods[m].base;

      ret = epwm_configure_module(m, DEFAULT_PWM_FREQ);
      if (ret < 0)
        {
          return ret;
        }

      /* Configure Action Qualifiers: Active-HIGH PWM
       * CTR = 0: Output goes HIGH
       * CTR = CMPA (or CMPB): Output goes LOW
       */

      epwm_write16(base, AM67A_EPWM_AQCTLA_OFFSET,
                   AQCTL_ZRO_SETHIGH | AQCTL_CAU_CLEARLOW);
      epwm_write16(base, AM67A_EPWM_AQCTLB_OFFSET,
                   AQCTL_ZRO_SETHIGH | AQCTL_CBU_CLEARLOW);

      /* Force outputs LOW initially (continuous software force) */

      epwm_modify16(base, AM67A_EPWM_AQSFRC_OFFSET,
                    AQSFRC_RLDCSF_MASK, AQSFRC_RLDCSF_IMDT);
      epwm_write16(base, AM67A_EPWM_AQCSFRC_OFFSET,
                   AQCSFRC_CSFA_LOW | AQCSFRC_CSFB_LOW);

      /* Zero compare registers */

      epwm_write16(base, AM67A_EPWM_CMPA_OFFSET, 0);
      epwm_write16(base, AM67A_EPWM_CMPB_OFFSET, 0);

      g_epwm_mods[m].initialized = true;
    }

  /* 4. Initialize ECAP0, ECAP1, ECAP2 in APWM mode (initially stopped/LOW) */

  for (e = 0; e < NUM_ECAP_MODULES; e++)
    {
      ecap_configure(e, DEFAULT_PWM_FREQ);
      ecap_set_duty_us(e, 0);
    }

  s_initialized = true;
  return OK;
}

/****************************************************************************
 * Name: am67a_mcu_pwm_set_freq
 ****************************************************************************/

int am67a_mcu_pwm_set_freq(int channel, uint32_t freq_hz)
{
  if (channel < 0 || channel >= AM67A_MCU_PWM_NCHANNELS)
    {
      return -EINVAL;
    }

  if (channel >= NUM_EPWM_CHANNELS)
    {
      return ecap_configure(channel - NUM_EPWM_CHANNELS, freq_hz);
    }

  int mod_idx = g_epwm_chans[channel].mod_idx;
  int ret = epwm_configure_module(mod_idx, freq_hz);
  if (ret < 0)
    {
      return ret;
    }

  /* Refresh active duty cycles on this module */

  int c;
  for (c = 0; c < NUM_EPWM_CHANNELS; c++)
    {
      if (g_epwm_chans[c].mod_idx == mod_idx && g_epwm_chans[c].enabled)
        {
          am67a_mcu_pwm_set_duty_us(c, g_epwm_chans[c].pulse_us);
        }
    }

  return OK;
}

/****************************************************************************
 * Name: am67a_mcu_pwm_set_duty_us
 ****************************************************************************/

int am67a_mcu_pwm_set_duty_us(int channel, uint32_t pulse_us)
{
  if (channel < 0 || channel >= AM67A_MCU_PWM_NCHANNELS)
    {
      return -EINVAL;
    }

  if (channel >= NUM_EPWM_CHANNELS)
    {
      return ecap_set_duty_us(channel - NUM_EPWM_CHANNELS, pulse_us);
    }

  struct am67a_epwm_chan_s *chan = &g_epwm_chans[channel];
  struct am67a_epwm_mod_s *mod = &g_epwm_mods[chan->mod_idx];
  uintptr_t base = mod->base;

  chan->pulse_us = pulse_us;

  if (pulse_us == 0)
    {
      /* Force output LOW */

      chan->enabled = false;
      if (chan->subchan == 0)
        {
          epwm_modify16(base, AM67A_EPWM_AQCSFRC_OFFSET,
                        AQCSFRC_CSFA_MASK, AQCSFRC_CSFA_LOW);
          epwm_write16(base, AM67A_EPWM_CMPA_OFFSET, 0);
        }
      else
        {
          epwm_modify16(base, AM67A_EPWM_AQCSFRC_OFFSET,
                        AQCSFRC_CSFB_MASK, AQCSFRC_CSFB_LOW);
          epwm_write16(base, AM67A_EPWM_CMPB_OFFSET, 0);
        }
      return OK;
    }

  /* Compute compare counts from microseconds */

  uint64_t counts = ((uint64_t)AM67A_MCU_EPWM_CLK_FREQ * pulse_us) /
                    (1000000ull * (uint64_t)mod->ps_div);

  if (counts > mod->tbprd)
    {
      counts = mod->tbprd;
    }

  if (chan->subchan == 0)
    {
      epwm_write16(base, AM67A_EPWM_CMPA_OFFSET, (uint16_t)counts);
      /* Disable continuous software force to let Action Qualifier run */
      epwm_modify16(base, AM67A_EPWM_AQCSFRC_OFFSET,
                    AQCSFRC_CSFA_MASK, AQCSFRC_CSFA_DIS);
    }
  else
    {
      epwm_write16(base, AM67A_EPWM_CMPB_OFFSET, (uint16_t)counts);
      epwm_modify16(base, AM67A_EPWM_AQCSFRC_OFFSET,
                    AQCSFRC_CSFB_MASK, AQCSFRC_CSFB_DIS);
    }

  chan->enabled = true;
  return OK;
}

/****************************************************************************
 * Name: am67a_mcu_pwm_stop
 ****************************************************************************/

int am67a_mcu_pwm_stop(int channel)
{
  return am67a_mcu_pwm_set_duty_us(channel, 0);
}

/****************************************************************************
 * Name: am67a_mcu_pwm_stop_all
 ****************************************************************************/

void am67a_mcu_pwm_stop_all(void)
{
  int c;
  for (c = 0; c < AM67A_MCU_PWM_NCHANNELS; c++)
    {
      am67a_mcu_pwm_stop(c);
    }
}

#ifdef CONFIG_PWM
/****************************************************************************
 * NuttX Lower-Half PWM Driver Implementation
 ****************************************************************************/

static int pwm_setup(FAR struct pwm_lowerhalf_s *dev)
{
  return am67a_mcu_pwm_init();
}

#define NUM_TOTAL_PWM_DEVS (NUM_EPWM_MODULES + NUM_ECAP_MODULES)

static int pwm_shutdown(FAR struct pwm_lowerhalf_s *dev)
{
  FAR struct am67a_pwm_dev_s *priv = (FAR struct am67a_pwm_dev_s *)dev;
  if (priv->mod_idx >= NUM_EPWM_MODULES)
    {
      return ecap_set_duty_us(priv->mod_idx - NUM_EPWM_MODULES, 0);
    }

  am67a_mcu_pwm_stop(priv->mod_idx * 2);
  am67a_mcu_pwm_stop(priv->mod_idx * 2 + 1);
  return OK;
}

static int pwm_start(FAR struct pwm_lowerhalf_s *dev,
                     FAR const struct pwm_info_s *info)
{
  FAR struct am67a_pwm_dev_s *priv = (FAR struct am67a_pwm_dev_s *)dev;
  int mod = priv->mod_idx;

  if (mod >= NUM_EPWM_MODULES)
    {
      int ecap_idx = mod - NUM_EPWM_MODULES;
      if (info->frequency > 0)
        {
          ecap_configure(ecap_idx, info->frequency);
        }

      uint32_t pulse_us;
#ifdef CONFIG_PWM_MULTICHAN
      pulse_us = (uint32_t)(((uint64_t)info->channels[0].duty * 1000000ull) /
                   (info->frequency * 65536ull));
#else
      pulse_us = (uint32_t)(((uint64_t)info->duty * 1000000ull) /
                   (info->frequency * 65536ull));
#endif
      return ecap_set_duty_us(ecap_idx, pulse_us);
    }

  if (info->frequency > 0)
    {
      epwm_configure_module(mod, info->frequency);
    }

#ifdef CONFIG_PWM_MULTICHAN
  int i;
  for (i = 0; i < CONFIG_PWM_NCHANNELS; i++)
    {
      int ch_num = info->channels[i].channel;
      if (ch_num <= 0)
        {
          break;
        }

      /* Map ch_num (1 or 2) to subchannel A or B */
      int subchan = ch_num - 1;
      if (subchan == 0 || subchan == 1)
        {
          uint32_t pulse_us = (uint32_t)(((uint64_t)info->channels[i].duty *
                               1000000ull) / (info->frequency * 65536ull));
          am67a_mcu_pwm_set_duty_us(mod * 2 + subchan, pulse_us);
        }
    }
#else
  /* Single channel mode: controls sub-channel A */
  uint32_t pulse_us = (uint32_t)(((uint64_t)info->duty * 1000000ull) /
                       (info->frequency * 65536ull));
  am67a_mcu_pwm_set_duty_us(mod * 2, pulse_us);
#endif

  return OK;
}

static int pwm_stop(FAR struct pwm_lowerhalf_s *dev)
{
  FAR struct am67a_pwm_dev_s *priv = (FAR struct am67a_pwm_dev_s *)dev;
  if (priv->mod_idx >= NUM_EPWM_MODULES)
    {
      return ecap_set_duty_us(priv->mod_idx - NUM_EPWM_MODULES, 0);
    }

  am67a_mcu_pwm_stop(priv->mod_idx * 2);
  am67a_mcu_pwm_stop(priv->mod_idx * 2 + 1);
  return OK;
}

static int pwm_ioctl(FAR struct pwm_lowerhalf_s *dev, int cmd,
                     unsigned long arg)
{
  return -ENOTTY;
}

static const struct pwm_ops_s g_pwm_ops =
{
  .setup    = pwm_setup,
  .shutdown = pwm_shutdown,
  .start    = pwm_start,
  .stop     = pwm_stop,
  .ioctl    = pwm_ioctl,
};

static struct am67a_pwm_dev_s g_pwm_devs[NUM_TOTAL_PWM_DEVS] =
{
  {
    .lower   = { &g_pwm_ops },
    .mod_idx = 0, /* EPWM0 -> /dev/pwm0 (Hat Pin 29 & Pin  8) */
  },
  {
    .lower   = { &g_pwm_ops },
    .mod_idx = 1, /* EPWM1 -> /dev/pwm1 (Hat Pin 31 & Pin 33) */
  },
  {
    .lower   = { &g_pwm_ops },
    .mod_idx = 2, /* ECAP0 -> /dev/pwm2 (Hat Pin 32) */
  },
  {
    .lower   = { &g_pwm_ops },
    .mod_idx = 3, /* ECAP1 -> /dev/pwm3 (Hat Pin 36) */
  },
  {
    .lower   = { &g_pwm_ops },
    .mod_idx = 4, /* ECAP2 -> /dev/pwm4 (Hat Pin 12) */
  },
};

FAR struct pwm_lowerhalf_s *am67a_mcu_pwminit(int timer)
{
  if (timer < 0 || timer >= NUM_TOTAL_PWM_DEVS)
    {
      return NULL;
    }

  return (FAR struct pwm_lowerhalf_s *)&g_pwm_devs[timer];
}
#endif /* CONFIG_PWM */
