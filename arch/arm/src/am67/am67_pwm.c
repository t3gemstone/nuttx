/****************************************************************************
 * arch/arm/src/am67/am67_pwm.c
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
#include <stdbool.h>
#include <stdint.h>

#include <nuttx/timers/pwm.h>

#include "arm_internal.h"
#include "am67_pinmux.h"
#include "am67_pwm.h"
#include "am67_pwm_hw.h"

#ifdef CONFIG_AM67_EPWM0

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MAIN_CTRL_MMR partition 1 lock (kick) registers */

#define AM67_MAIN_CTRL_MMR_BASE           0x00100000
#define AM67_CTRL_MMR_LOCK1_KICK0         0x5008      /* Offset from base */
#define AM67_CTRL_MMR_LOCK1_KICK1         0x500c
#define AM67_CTRL_MMR_KICK0_UNLOCK_KEY    0x68ef3490
#define AM67_CTRL_MMR_KICK0_UNLOCKED      (1u << 0)   /* 1 = unlocked */
#define AM67_CTRL_MMR_KICK1_UNLOCK_KEY    0xd172bc5a

/* EPWM time-base clock gate (CTRL_MMR partition 1) */

#define AM67_CTRL_MMR_EPWM_TB_CLKEN       0x4130      /* Offset from base */
#define AM67_EPWM_TB_CLKEN_EPWM0_EN       (1u << 0)
#define AM67_EPWM_TB_CLKEN_EPWM1_EN       (1u << 1)
#define AM67_EPWM_TB_CLKEN_EPWM2_EN       (1u << 2)

/* EPWM functional clock (FICLK).  Verified on copper 2026-07-16: with
 * HSPCLKDIV=1, CLKDIV=4, TBPRD=62499 the pin measured 1.000 kHz, which
 * pins FICLK to 250 MHz within 0.1%.
 */

#define AM67_EPWM_FICLK_HZ                250000000u

/* The time-base counter is 16-bit: one period is TBPRD+1 ticks, at most
 * 65536.  The 2-tick floor (TBPRD=1) is a deliberate hardware-limit
 * policy: the driver does not cap the user's frequency below what the
 * silicon can express, but duty resolution degrades as ticks shrink
 * (at the 125 MHz ceiling only 0/50/100% duty exist).
 */

#define AM67_EPWM_MAX_TICKS               65536u
#define AM67_EPWM_MIN_TICKS               2u

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* PWM lower-half state.  Must begin with the ops pointer so that this
 * structure can be cast to and from struct pwm_lowerhalf_s.  frequency
 * and tbprd cache the running wave (frequency == 0 means no wave is
 * running; see am67_epwm_stop()).
 */

struct am67_epwm_s
{
  const struct pwm_ops_s *ops;
  uint32_t frequency;
  ub16_t duty;
  uint16_t tbprd;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* PWM driver methods */

static int am67_epwm_setup(struct pwm_lowerhalf_s *dev);
static int am67_epwm_shutdown(struct pwm_lowerhalf_s *dev);
static int am67_epwm_start(struct pwm_lowerhalf_s *dev,
                           const struct pwm_info_s *info);
static int am67_epwm_stop(struct pwm_lowerhalf_s *dev);
static int am67_epwm_ioctl(struct pwm_lowerhalf_s *dev,
                           int cmd, unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct pwm_ops_s g_am67_epwmops =
{
  .setup       = am67_epwm_setup,
  .shutdown    = am67_epwm_shutdown,
  .start       = am67_epwm_start,
  .stop        = am67_epwm_stop,
  .ioctl       = am67_epwm_ioctl,
};

static struct am67_epwm_s g_am67_epwm =
{
  .ops = &g_am67_epwmops,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67_epwm_getreg
 *
 * Description:
 *   Get a 32-bit register value by offset.
 *
 ****************************************************************************/

static inline uint32_t am67_epwm_getreg(uint32_t base, uint32_t offset)
{
  return getreg32(base + offset);
}

/****************************************************************************
 * Name: am67_epwm_putreg
 *
 * Description:
 *   Put a 32-bit register value by offset.
 *
 ****************************************************************************/

static inline void am67_epwm_putreg(uint32_t base, uint32_t offset,
                                    uint32_t value)
{
  putreg32(value, base + offset);
}

/****************************************************************************
 * Name: am67_epwm_getreg16
 *
 * Description:
 *   Get a 16-bit register value by offset.  All EPWM core registers
 *   except PID are 16-bit; 32-bit access is unaligned for half of them
 *   and clobbers the neighboring register for the rest.
 *
 ****************************************************************************/

static inline uint16_t am67_epwm_getreg16(uint32_t base, uint32_t offset)
{
  return getreg16(base + offset);
}

/****************************************************************************
 * Name: am67_epwm_putreg16
 *
 * Description:
 *   Put a 16-bit register value by offset.
 *
 ****************************************************************************/

static inline void am67_epwm_putreg16(uint32_t base, uint32_t offset,
                                      uint16_t value)
{
  putreg16(value, base + offset);
}

/****************************************************************************
 * Name: am67_epwm_enable_register_write
 *
 * Description:
 *   Unlock partition 1 of the MAIN_CTRL_MMR (kick lock) so that the EPWM
 *   time-base clock gate register can be written.  The KICK0 status bit
 *   is read back to confirm that the partition is unlocked.
 *
 * Returned Value:
 *   Zero (OK) on success; -EIO if the partition is still locked after
 *   writing the unlock keys.
 *
 ****************************************************************************/

static int am67_epwm_enable_register_write(void)
{
  uint32_t regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                                     AM67_CTRL_MMR_LOCK1_KICK0);

  if ((regval & AM67_CTRL_MMR_KICK0_UNLOCKED) == 0u)
    {
      am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE, AM67_CTRL_MMR_LOCK1_KICK0,
                       AM67_CTRL_MMR_KICK0_UNLOCK_KEY);
      am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE, AM67_CTRL_MMR_LOCK1_KICK1,
                       AM67_CTRL_MMR_KICK1_UNLOCK_KEY);
    }

  regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                            AM67_CTRL_MMR_LOCK1_KICK0);

  if ((regval & AM67_CTRL_MMR_KICK0_UNLOCKED) == 0u)
    {
      pwmerr("ERROR: Could not unlock CTRL_MMR partition 1\n");
      return -EIO;
    }

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_enable_clock
 *
 * Description:
 *   Enable the EPWM0 time-base clock in the EPWM_TB_CLKEN register.
 *   Read-modify-write to preserve the gates of the other EPWM instances
 *   (EPWM2 drives the board cooling fan).  Read back to verify that the
 *   gate bit stuck.
 *
 * Returned Value:
 *   Zero (OK) on success; -EIO if the clock gate did not enable.
 *
 ****************************************************************************/

static int am67_epwm_enable_clock(void)
{
  uint32_t regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                                     AM67_CTRL_MMR_EPWM_TB_CLKEN);

  regval |= AM67_EPWM_TB_CLKEN_EPWM0_EN;

  am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE, AM67_CTRL_MMR_EPWM_TB_CLKEN,
                   regval);

  regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                            AM67_CTRL_MMR_EPWM_TB_CLKEN);

  if ((regval & AM67_EPWM_TB_CLKEN_EPWM0_EN) == 0u)
    {
      pwmerr("ERROR: Could not enable EPWM0 clock: TB_CLKEN: 0x%08" PRIx32
             "\n", regval);
      return -EIO;
    }

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_disable_clock
 *
 * Description:
 *   Gate the EPWM0 time-base clock off.  Read-modify-write: bit 2 of the
 *   shared TB_CLKEN register is the cooling fan (EPWM2) and must not be
 *   disturbed.  Read back to verify.
 *
 * Returned Value:
 *   Zero (OK) on success; -EIO if the clock gate did not disable.
 *
 ****************************************************************************/

static int am67_epwm_disable_clock(void)
{
  uint32_t regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                                     AM67_CTRL_MMR_EPWM_TB_CLKEN);

  regval &= ~AM67_EPWM_TB_CLKEN_EPWM0_EN;

  am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE, AM67_CTRL_MMR_EPWM_TB_CLKEN,
                   regval);

  regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                            AM67_CTRL_MMR_EPWM_TB_CLKEN);

  if ((regval & AM67_EPWM_TB_CLKEN_EPWM0_EN) != 0u)
    {
      pwmerr("ERROR: Could not disable EPWM0 clock: TB_CLKEN: 0x%08" PRIx32
             "\n", regval);
      return -EIO;
    }

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_check_pid
 *
 * Description:
 *   Read the EPWM peripheral ID register and compare it against the
 *   expected value.  Verifies that the module is powered and that the
 *   base address is correct.  An unpowered domain reads zeros without
 *   any bus fault, so PID == 0 means "not powered".
 *
 * Returned Value:
 *   Zero (OK) on success; -EIO if the PID does not match.
 *
 ****************************************************************************/

static int am67_epwm_check_pid(void)
{
  uint32_t regval = am67_epwm_getreg(AM67_EPWM0_BASE,
                                     AM67_EPWM_PID_OFFSET);

  if (regval != AM67_EPWM_PID_EXPECTED)
    {
      pwmerr("ERROR: Unexpected EPWM PID: 0x%08" PRIx32
             " (expected 0x%08" PRIx32 ")\n",
             regval, (uint32_t)AM67_EPWM_PID_EXPECTED);
      return -EIO;
    }

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_config_aqctla
 *
 * Description:
 *   Program the action qualifier for output A: SET at counter zero,
 *   CLEAR at CMPA on the way up (up-count asymmetric recipe, duty is
 *   proportional to CMPA).
 *
 ****************************************************************************/

static void am67_epwm_config_aqctla(void)
{
  uint16_t regval = am67_epwm_getreg16(AM67_EPWM0_BASE,
                                       AM67_EPWM_AQCTLA_OFFSET);

  regval |= (AM67_EPWM_AQ_SET << AM67_EPWM_AQCTLA_ZRO_SHIFT);
  regval |= (AM67_EPWM_AQ_CLEAR << AM67_EPWM_AQCTLA_CAU_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_AQCTLA_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_clear_aqctla
 *
 * Description:
 *   Return the output A action qualifier to its reset state (no actions
 *   on any event).  Part of the shutdown teardown.
 *
 ****************************************************************************/

static void am67_epwm_clear_aqctla(void)
{
  uint16_t regval = am67_epwm_getreg16(AM67_EPWM0_BASE,
                                       AM67_EPWM_AQCTLA_OFFSET);

  regval &= ~AM67_EPWM_AQCTLA_ZRO_MASK;
  regval &= ~AM67_EPWM_AQCTLA_CAU_MASK;

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_AQCTLA_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_immediate_force_low
 *
 * Description:
 *   Park the pin low via continuous software force.  RLDCSF must be set
 *   to immediate first: with the counter frozen the AQCSFRC shadow would
 *   never load and the force would silently never take effect.  The
 *   force is a mask in front of the AQ output latch, not a write to it;
 *   releasing it exposes whatever the latch last held.
 *
 ****************************************************************************/

static void am67_epwm_immediate_force_low(void)
{
  uint16_t regval = am67_epwm_getreg16(AM67_EPWM0_BASE,
                                       AM67_EPWM_AQSFRC_OFFSET);

  regval &= ~AM67_EPWM_AQSFRC_RLDCSF_MASK;
  regval |= (AM67_EPWM_AQSFRC_RLDCSF_IMMEDIATE <<
             AM67_EPWM_AQSFRC_RLDCSF_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_AQSFRC_OFFSET, regval);

  regval = am67_epwm_getreg16(AM67_EPWM0_BASE, AM67_EPWM_AQCSFRC_OFFSET);

  regval &= ~AM67_EPWM_AQCSFRC_CSFA_MASK;
  regval |= (AM67_EPWM_CSFA_FORCE_LOW << AM67_EPWM_AQCSFRC_CSFA_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_AQCSFRC_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_immediate_force_disable
 *
 * Description:
 *   Release the continuous software force so the pin follows the AQ
 *   output again.  Called only after ignition: while the counter runs,
 *   the AQ latch is live wave state, so the release exposes the true
 *   waveform and not a stale level.  On a fresh start the force is
 *   already disabled and this is a harmless no-op.
 *
 ****************************************************************************/

static void am67_epwm_immediate_force_disable(void)
{
  uint16_t regval = am67_epwm_getreg16(AM67_EPWM0_BASE,
                                       AM67_EPWM_AQSFRC_OFFSET);

  regval &= ~AM67_EPWM_AQSFRC_RLDCSF_MASK;
  regval |= (AM67_EPWM_AQSFRC_RLDCSF_IMMEDIATE <<
             AM67_EPWM_AQSFRC_RLDCSF_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_AQSFRC_OFFSET, regval);

  regval = am67_epwm_getreg16(AM67_EPWM0_BASE, AM67_EPWM_AQCSFRC_OFFSET);

  regval &= ~AM67_EPWM_AQCSFRC_CSFA_MASK;
  regval |= (AM67_EPWM_CSFA_FORCE_DISABLE << AM67_EPWM_AQCSFRC_CSFA_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_AQCSFRC_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_set_tbctl
 *
 * Description:
 *   Compose the static TBCTL policy with the counter frozen: CTRMODE =
 *   stop-freeze (ignition is start()'s separate final step), PRDLD
 *   immediate (a shadowed TBPRD would wait for a zero event that a
 *   frozen counter never generates), SYNCO off.  Full compose, not RMW:
 *   this function owns every policy field.  The frequency-dependent
 *   fields (HSPCLKDIV/CLKDIV) belong to am67_epwm_set_clock_values().
 *
 ****************************************************************************/

static void am67_epwm_set_tbctl(void)
{
  uint16_t regval = (AM67_EPWM_TBCTL_CTRMODE_STOP_FREEZE <<
                     AM67_EPWM_TBCTL_CTRMODE_SHIFT);

  regval |= AM67_EPWM_TBCTL_PRDLD_IMMEDIATE;
  regval |= (3u << AM67_EPWM_TBCTL_SYNCOSEL_SHIFT);  /* 3 = SYNCO off */

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_set_cmpctl
 *
 * Description:
 *   Configure counter-compare: CMPA shadowed (SHDWAMODE=0), shadow
 *   loaded into the active register on the PRD event (LOADAMODE=1) so a
 *   duty update never races the SET action at counter zero.
 *
 ****************************************************************************/

static void am67_epwm_set_cmpctl(void)
{
  uint16_t regval = 0u;

  regval |= (1u << AM67_EPWM_CMPCTL_LOADAMODE_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_CMPCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_clear_cmpctl
 *
 * Description:
 *   Return CMPCTL to its reset state (all fields zero).
 *
 ****************************************************************************/

static void am67_epwm_clear_cmpctl(void)
{
  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_CMPCTL_OFFSET, 0u);
}

/****************************************************************************
 * Name: am67_epwm_tbctl_ctrmode_freeze
 *
 * Description:
 *   Freeze the time-base counter (CTRMODE = stop-freeze).  Freeze holds
 *   the current count and pin level; it does not clear them.
 *
 ****************************************************************************/

static void am67_epwm_tbctl_ctrmode_freeze(void)
{
  uint16_t regval = am67_epwm_getreg16(AM67_EPWM0_BASE,
                                       AM67_EPWM_TBCTL_OFFSET);

  regval &= ~AM67_EPWM_TBCTL_CTRMODE_MASK;
  regval |= (AM67_EPWM_TBCTL_CTRMODE_STOP_FREEZE <<
             AM67_EPWM_TBCTL_CTRMODE_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_tbctl_ctrmode_up
 *
 * Description:
 *   Ignition: flip CTRMODE from freeze to up-count with everything else
 *   already configured.  Clear-then-set on the live register.
 *
 ****************************************************************************/

static void am67_epwm_tbctl_ctrmode_up(void)
{
  uint16_t regval = am67_epwm_getreg16(AM67_EPWM0_BASE,
                                       AM67_EPWM_TBCTL_OFFSET);

  regval &= ~AM67_EPWM_TBCTL_CTRMODE_MASK;
  regval |= (AM67_EPWM_TBCTL_CTRMODE_UP << AM67_EPWM_TBCTL_CTRMODE_SHIFT);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_reset_tbcnt
 *
 * Description:
 *   Zero the time-base counter (safe while frozen; clears stale count).
 *
 ****************************************************************************/

static void am67_epwm_reset_tbcnt(void)
{
  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_TBCNT_OFFSET, 0u);
}

/****************************************************************************
 * Name: am67_epwm_calculate_clock_values
 *
 * Description:
 *   Pure divider solver (no register access, no driver state): map a
 *   requested pin frequency onto the EPWM clock tree,
 *
 *     f_pin = FICLK / (hsp * clk * (tbprd + 1))
 *
 *   hsp and clk are returned as divider values (1..14 / 1..128); the
 *   register field encodings (2n vs 2^n) are deliberately kept out of
 *   the math.  ticks = FICLK / frequency truncates: worst-case error is
 *   under one tick, ppm-level at any frequency this driver serves.
 *
 *   Selection policy: must fit in 16-bit TBPRD; prefer exact division
 *   (zero frequency error); among equals prefer the smallest total
 *   divider (largest TBPRD+1 = finest duty resolution).
 *
 * Returned Value:
 *   OK on success; -ERANGE if the frequency is 0, above the 125 MHz
 *   2-tick ceiling, or below the ~2.13 Hz divide-by-1792 floor.
 *
 ****************************************************************************/

static int am67_epwm_calculate_clock_values(uint32_t frequency,
                                            uint16_t *hsp, uint16_t *clk,
                                            uint16_t *tbprd)
{
  /* The silicon's divider menus: HSPCLKDIV is even steps, CLKDIV is
   * powers of two.  Only pairs from these menus are representable.
   */

  static const uint16_t hsp_menu[] =
  {
    1, 2, 4, 6, 8, 10, 12, 14
  };

  static const uint16_t clk_menu[] =
  {
    1, 2, 4, 8, 16, 32, 64, 128
  };

  uint32_t ticks;
  uint32_t best_div = 0;
  bool best_exact = false;
  int i;
  int j;

  if (frequency == 0u)
    {
      return -ERANGE;
    }

  ticks = AM67_EPWM_FICLK_HZ / frequency;

  if (ticks < AM67_EPWM_MIN_TICKS)
    {
      return -ERANGE;
    }

  if (ticks <= AM67_EPWM_MAX_TICKS)
    {
      *hsp   = 1;
      *clk   = 1;
      *tbprd = (uint16_t)(ticks - 1u);
      return OK;
    }

  /* Too many ticks for a 16-bit period: brute-force all 64 menu pairs.
   * 64 iterations of integer math beats clever factoring for both
   * correctness and readability.
   */

  for (i = 0; i < 8; i++)
    {
      for (j = 0; j < 8; j++)
        {
          uint32_t div = (uint32_t)hsp_menu[i] * clk_menu[j];
          bool exact;

          if (ticks > div * AM67_EPWM_MAX_TICKS)
            {
              continue; /* Does not fit in TBPRD with this divider */
            }

          exact = ((ticks % div) == 0u);

          if (best_div == 0u ||
              (exact && !best_exact) ||
              (exact == best_exact && div < best_div))
            {
              best_div   = div;
              best_exact = exact;
              *hsp       = hsp_menu[i];
              *clk       = clk_menu[j];
            }
        }
    }

  if (best_div == 0u)
    {
      return -ERANGE;
    }

  *tbprd = (uint16_t)(ticks / best_div - 1u);
  return OK;
}

/****************************************************************************
 * Name: am67_epwm_loop_log2
 *
 * Description:
 *   Integer log2 of a power of two (CLKDIV divider value to register
 *   field code: 1->0, 2->1, ... 128->7).
 *
 ****************************************************************************/

static uint16_t am67_epwm_loop_log2(uint16_t value)
{
  uint16_t result = 0;

  while (value >>= 1)
    {
      result++;
    }

  return result;
}

/****************************************************************************
 * Name: am67_epwm_set_clock_values
 *
 * Description:
 *   Write the solver's results to the hardware: TBPRD, then HSPCLKDIV
 *   and CLKDIV (clear-then-set: the register is live and the fields may
 *   need to shrink, which OR alone cannot do).  This is the only place
 *   that knows the divider field encodings: HSPCLKDIV is div/2 (with
 *   1 -> 0 falling out of integer division), CLKDIV is log2.
 *
 ****************************************************************************/

static void am67_epwm_set_clock_values(uint16_t hsp, uint16_t clk,
                                       uint16_t tbprd)
{
  uint16_t regval;

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_TBPRD_OFFSET, tbprd);

  regval = am67_epwm_getreg16(AM67_EPWM0_BASE, AM67_EPWM_TBCTL_OFFSET);

  regval &= ~AM67_EPWM_TBCTL_HSPCLKDIV_MASK;
  regval |= (hsp / 2) << AM67_EPWM_TBCTL_HSPCLKDIV_SHIFT;

  regval &= ~AM67_EPWM_TBCTL_CLKDIV_MASK;
  regval |= am67_epwm_loop_log2(clk) << AM67_EPWM_TBCTL_CLKDIV_SHIFT;

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_set_duty
 *
 * Description:
 *   Convert the ub16 duty fraction to CMPA ticks and write it.  The
 *   write lands in the CMPA shadow register and loads at the next PRD
 *   event (glitch-free live update).  Worst case product is
 *   65535 * 65536 < 2^32: no overflow in 32-bit math.  duty = 0 gives
 *   exact 0% (compare outranks zero in AQ priority); exact 100% is
 *   unreachable by the ub16 format itself (max 65535/65536).
 *
 ****************************************************************************/

static void am67_epwm_set_duty(struct pwm_lowerhalf_s *dev, ub16_t duty)
{
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;
  uint16_t cmpa = (uint16_t)((duty * (priv->tbprd + 1u)) >> 16);

  am67_epwm_putreg16(AM67_EPWM0_BASE, AM67_EPWM_CMPA_OFFSET, cmpa);
}

/****************************************************************************
 * Name: am67_epwm_setup
 *
 * Description:
 *   Called by the upper half on the first open of /dev/pwm0.  Bring the
 *   module to a configured-but-silent state: clock on, PID sanity
 *   check, pinmux, and the parameter-independent policies (CMPCTL,
 *   AQCTLA, frozen TBCTL).  No pulses until start().
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno from the first failing step.
 *
 ****************************************************************************/

static int am67_epwm_setup(struct pwm_lowerhalf_s *dev)
{
  int ret;

  ret = am67_epwm_enable_clock();
  if (ret < 0)
    {
      return ret;
    }

  ret = am67_epwm_check_pid();
  if (ret < 0)
    {
      return ret;
    }

  am67_epwm_pinmux_init();
  am67_epwm_set_cmpctl();
  am67_epwm_config_aqctla();
  am67_epwm_set_tbctl();

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_shutdown
 *
 * Description:
 *   Called by the upper half on the last close.  Teardown in reverse
 *   order of setup: park the pin (stop handles a still-running wave for
 *   free), clear the policy registers, then gate the clock off last.
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int am67_epwm_shutdown(struct pwm_lowerhalf_s *dev)
{
  am67_epwm_stop(dev);
  am67_epwm_clear_aqctla();
  am67_epwm_clear_cmpctl();

  return am67_epwm_disable_clock();
}

/****************************************************************************
 * Name: am67_epwm_start
 *
 * Description:
 *   Start (or update) the pulsed output.  Three-phase transaction:
 *
 *   VALIDATE: duty range, then the pure divider solver.  Any failure
 *     returns here with neither hardware nor cached state touched.
 *   ACT: same frequency is a single shadowed CMPA write (glitch-free);
 *     otherwise stop first (parks the pin, makes re-entry identical to
 *     a fresh start), write dividers and period, ignite, and release
 *     the software force only after ignition.
 *   COMMIT: cache the new wave in priv (done just before ACT's register
 *     writes because set_duty derives CMPA from priv->tbprd; nothing
 *     after the commit point can fail).
 *
 *   The first period after (re)ignition runs with the previous active
 *   CMPA (0 on a fresh start = one silent period): accepted, safe-low.
 *
 * Returned Value:
 *   Zero (OK) on success; -EINVAL for an out-of-range duty; -ERANGE if
 *   the frequency cannot be produced.
 *
 ****************************************************************************/

static int am67_epwm_start(struct pwm_lowerhalf_s *dev,
                           const struct pwm_info_s *info)
{
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;
  uint16_t hsp = 0;
  uint16_t clk = 0;
  uint16_t tbprd = 0;
  int ret;

  if (info->duty > 0xffff)
    {
      return -EINVAL;
    }

  /* Same frequency: shadowed CMPA write only, no stop needed */

  if (priv->frequency == info->frequency)
    {
      am67_epwm_set_duty(dev, info->duty);
      priv->duty = info->duty;
      return OK;
    }

  ret = am67_epwm_calculate_clock_values(info->frequency, &hsp, &clk,
                                         &tbprd);
  if (ret < 0)
    {
      pwmerr("ERROR: Cannot produce %" PRIu32 " Hz, keeping %" PRIu32
             " Hz\n", info->frequency, priv->frequency);
      return ret;
    }

  /* Commit: nothing below can fail (void register writes only), and
   * set_duty derives CMPA from priv->tbprd, so priv must be current
   * before the writes begin.
   */

  priv->frequency = info->frequency;
  priv->tbprd     = tbprd;
  priv->duty      = info->duty;

  am67_epwm_stop(dev);
  am67_epwm_set_clock_values(hsp, clk, tbprd);
  am67_epwm_reset_tbcnt();
  am67_epwm_set_duty(dev, info->duty);
  am67_epwm_tbctl_ctrmode_up();
  am67_epwm_immediate_force_disable();

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_stop
 *
 * Description:
 *   Park the output: force the pin low (RLDCSF first), then freeze the
 *   counter.  The order is load-bearing: the force masks the pin before
 *   the freeze fossilizes the AQ latch at an arbitrary level.  The
 *   cached frequency is invalidated (0 = no wave) so that a subsequent
 *   start of the same frequency cannot take the CMPA-only fast path on
 *   a frozen module.
 *
 * Returned Value:
 *   Zero (OK) always.
 *
 ****************************************************************************/

static int am67_epwm_stop(struct pwm_lowerhalf_s *dev)
{
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;

  am67_epwm_immediate_force_low();
  am67_epwm_tbctl_ctrmode_freeze();

  priv->frequency = 0;

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_ioctl
 *
 * Description:
 *   No platform-specific ioctl commands are supported.
 *
 * Returned Value:
 *   -ENOTTY always.
 *
 ****************************************************************************/

static int am67_epwm_ioctl(struct pwm_lowerhalf_s *dev,
                           int cmd, unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67_epwminitialize
 *
 * Description:
 *   Return the EPWM lower-half instance for the given PWM number so the
 *   board bringup can bind it to the upper half with pwm_register().
 *   No hardware is touched here.
 *
 * Input Parameters:
 *   pwm - PWM instance number; only 0 (EPWM0) exists today.
 *
 * Returned Value:
 *   Pointer to the lower-half driver on success; NULL on an unsupported
 *   instance number.
 *
 ****************************************************************************/

struct pwm_lowerhalf_s *am67_epwminitialize(int pwm)
{
  if (pwm != 0)
    {
      pwmerr("ERROR: No such PWM instance: %d\n", pwm);
      return NULL;
    }

  pwminfo("Initialize EPWM%d\n", pwm);

  return (struct pwm_lowerhalf_s *)&g_am67_epwm;
}

/****************************************************************************
 * Name: am67_epwm_init
 *
 * Description:
 *   Boot-time EPWM preparation: unlock CTRL_MMR partition 1 (kick lock)
 *   so that the clock-gate and pad writes issued later by the PWM
 *   lower-half setup() can land.  Everything else (clock enable, PID
 *   check, pinmux, waveform) is done by the lower-half ops, called by
 *   the upper half on open/ioctl.  Must run before pwm_register().
 *
 * Assumptions:
 *   The EPWM0 power domain must be on (it is managed by the DMSC/TISCI
 *   firmware, not by this driver).  On the current board it has been
 *   observed to stay on without any intervention (mechanism unconfirmed;
 *   possibly shared with the EPWM2 cooling fan or an unreaped boot
 *   default) - do NOT rely on this.  The only explicit guarantee today
 *   is forcing the device active from Linux before the R5F starts:
 *
 *     echo on > /sys/devices/platform/bus@f0000/23000000.pwm/power/control
 *
 *   ("on" disables Linux runtime power management for that device, which
 *   keeps the domain powered until reboot.)  If the domain is off, EPWM
 *   registers read as zeros - no bus fault - so a PID mismatch of
 *   0x00000000 means "not powered", not "wrong address".  Sub-word
 *   (8-bit) accesses also read as zeros; use 16/32-bit accesses only.
 *
 * WARNING: This power domain is currently held up by an unidentified
 *   party on the Linux side.  A Linux reboot or deliberate PM action
 *   (pwmchip unexport, driver unbind, 'echo auto') WILL release it, and
 *   if the holder is the EPWM2 fan's thermal policy, it could drop
 *   spontaneously (e.g. fan off when cool) - unverified but not
 *   excluded.  Either way the R5F gets no notification; outputs die
 *   silently.  Do not drive safety-critical loads (motors, ESCs) until
 *   a NuttX-side TISCI client owns this domain.
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value from the first failing
 *   step otherwise.
 *
 ****************************************************************************/

int am67_epwm_init(void)
{
  return am67_epwm_enable_register_write();
}

#endif /* CONFIG_AM67_EPWM0 */
