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
#include <syslog.h>

#include <nuttx/panic_notifier.h>
#include <nuttx/timers/pwm.h>

#include "arm_internal.h"
#include "am67_pinmux.h"
#include "am67_pwm.h"
#include "am67_pwm_hw.h"

#if defined(CONFIG_AM67_EPWM0) || defined(CONFIG_AM67_EPWM1)

#if CONFIG_PWM_NCHANNELS < 2
#  error "AM67 EPWM requires CONFIG_PWM_NCHANNELS >= 2"
#endif

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
#define AM67_CTRL_MMR_EPWM_CTRL(n)        (0x4140 + 4 * (n))   /* EPWMn_CTRL */
#define AM67_CTRL_MMR_EPWM_CTRL_EALLOW    (1u << 4)   /* trip-zone write enable */
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

/* Channel bookkeeping masks: channel number (1 = A, 2 = B) to its bit
 * in the active/requested/joiner sets.  CH_BIT(1) == CH_A_ACTIVE and
 * CH_BIT(2) == CH_B_ACTIVE by construction.
 */

#define CH_A_ACTIVE     (1u << 0)
#define CH_B_ACTIVE     (1u << 1)
#define CH_BIT(ch)      (1u << ((ch) - 1))

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* PWM lower-half state.  Must begin with the ops pointer so that this
 * structure can be cast to and from struct pwm_lowerhalf_s.  base,
 * clken_mask and pinmux_id identify the instance; frequency and tbprd
 * cache the running wave (frequency == 0 means no wave is running;
 * see am67_epwm_stop()).
 */

struct am67_epwm_s
{
  const struct pwm_ops_s *ops;
  uint32_t base;
  uint32_t clken_mask;      /* This instance's TB_CLKEN gate bit */
  uint32_t frequency;
  uint16_t tbprd;
  uint8_t active_channels;
  uint8_t pinmux_id;        /* Instance number for pad muxing */
  volatile bool live;       /* setup() done: module powered and clocked */
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
static int am67_epwm_panic(FAR struct notifier_block *nb,
                           unsigned long action, FAR void *data);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Time-base clock gates this core turned on, and how often another host
 * turned one of them off under it (am67_epwm_tbclk_guard()).
 */

static volatile uint32_t g_tbclk_owned;
static volatile uint32_t g_tbclk_restores;

static const struct pwm_ops_s g_am67_epwmops =
{
  .setup       = am67_epwm_setup,
  .shutdown    = am67_epwm_shutdown,
  .start       = am67_epwm_start,
  .stop        = am67_epwm_stop,
  .ioctl       = am67_epwm_ioctl,
};

#ifdef CONFIG_AM67_EPWM0
static struct am67_epwm_s g_am67_epwm0 =
{
  .ops        = &g_am67_epwmops,
  .base       = AM67_EPWM0_BASE,
  .clken_mask = AM67_EPWM_TB_CLKEN_EPWM0_EN,
  .pinmux_id  = 0,
};
#endif

#ifdef CONFIG_AM67_EPWM1
static struct am67_epwm_s g_am67_epwm1 =
{
  .ops        = &g_am67_epwmops,
  .base       = AM67_EPWM1_BASE,
  .clken_mask = AM67_EPWM_TB_CLKEN_EPWM1_EN,
  .pinmux_id  = 1,
};
#endif

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
 *   Enable one instance's time-base clock in the shared EPWM_TB_CLKEN
 *   register.  Read-modify-write to preserve the gates of the other
 *   EPWM instances (EPWM2 drives the board cooling fan).  Read back to
 *   verify that the gate bit stuck.
 *
 * Returned Value:
 *   Zero (OK) on success; -EIO if the clock gate did not enable.
 *
 ****************************************************************************/

static int am67_epwm_enable_clock(uint32_t clken_mask)
{
  uint32_t regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                                     AM67_CTRL_MMR_EPWM_TB_CLKEN);

  regval |= clken_mask;

  am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE, AM67_CTRL_MMR_EPWM_TB_CLKEN,
                   regval);

  regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                            AM67_CTRL_MMR_EPWM_TB_CLKEN);

  if ((regval & clken_mask) == 0u)
    {
      pwmerr("ERROR: Could not enable EPWM clock: TB_CLKEN: 0x%08" PRIx32
             "\n", regval);
      return -EIO;
    }

  g_tbclk_owned |= clken_mask;
  return OK;
}

/****************************************************************************
 * Name: am67_epwm_disable_clock
 *
 * Description:
 *   Gate one instance's time-base clock off.  Read-modify-write: bit 2
 *   of the shared TB_CLKEN register is the cooling fan (EPWM2) and must
 *   not be disturbed.  Read back to verify.
 *
 * Returned Value:
 *   Zero (OK) on success; -EIO if the clock gate did not disable.
 *
 ****************************************************************************/

static int am67_epwm_disable_clock(uint32_t clken_mask)
{
  uint32_t regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                                     AM67_CTRL_MMR_EPWM_TB_CLKEN);

  g_tbclk_owned &= ~clken_mask;
  regval &= ~clken_mask;

  am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE, AM67_CTRL_MMR_EPWM_TB_CLKEN,
                   regval);

  regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                            AM67_CTRL_MMR_EPWM_TB_CLKEN);

  if ((regval & clken_mask) != 0u)
    {
      pwmerr("ERROR: Could not disable EPWM clock: TB_CLKEN: 0x%08" PRIx32
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

static int am67_epwm_check_pid(uint32_t base)
{
  uint32_t regval = am67_epwm_getreg(base, AM67_EPWM_PID_OFFSET);

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
 * Name: am67_epwm_config_aqctl
 *
 * Description:
 *   High from the start of the period until the up-count compare: SET
 *   on zero and on period, CLEAR on this channel's compare. The period
 *   SET is one timebase tick at the end of the low interval. Full write
 *   so a read-modify cannot keep stale actions.
 *
 ****************************************************************************/

static void am67_epwm_config_aqctl(uint32_t base, int8_t channel)
{
  uint16_t regval;

  if (channel == 1)
    {
      regval  = (AM67_EPWM_AQ_SET << AM67_EPWM_AQCTLA_ZRO_SHIFT);
      regval |= (AM67_EPWM_AQ_SET << AM67_EPWM_AQCTLA_PRD_SHIFT);
      regval |= (AM67_EPWM_AQ_CLEAR << AM67_EPWM_AQCTLA_CAU_SHIFT);
      am67_epwm_putreg16(base, AM67_EPWM_AQCTLA_OFFSET, regval);
    }
  else
    {
      regval  = (AM67_EPWM_AQ_SET << AM67_EPWM_AQCTLB_ZRO_SHIFT);
      regval |= (AM67_EPWM_AQ_SET << AM67_EPWM_AQCTLB_PRD_SHIFT);
      regval |= (AM67_EPWM_AQ_CLEAR << AM67_EPWM_AQCTLB_CBU_SHIFT);
      am67_epwm_putreg16(base, AM67_EPWM_AQCTLB_OFFSET, regval);
    }
}

/****************************************************************************
 * Name: am67_epwm_clear_aqctl
 *
 * Description:
 *   Return one output's action qualifier to its reset state (no actions
 *   on any event).  Part of the shutdown teardown.
 *
 ****************************************************************************/

static void am67_epwm_clear_aqctl(uint32_t base, int8_t channel)
{
  uint32_t offset = (channel == 1) ? AM67_EPWM_AQCTLA_OFFSET
                                   : AM67_EPWM_AQCTLB_OFFSET;

  am67_epwm_putreg16(base, offset, 0u);
}

/****************************************************************************
 * Name: am67_epwm_rldcsf_immediate
 *
 * Description:
 *   Set the AQCSFRC load mode to immediate.  Must precede any force
 *   write while the counter may be frozen: otherwise the AQCSFRC shadow
 *   never loads and the force silently never takes effect.
 *
 ****************************************************************************/

static void am67_epwm_rldcsf_immediate(uint32_t base)
{
  uint16_t regval = am67_epwm_getreg16(base, AM67_EPWM_AQSFRC_OFFSET);

  regval &= ~AM67_EPWM_AQSFRC_RLDCSF_MASK;
  regval |= (AM67_EPWM_AQSFRC_RLDCSF_IMMEDIATE <<
             AM67_EPWM_AQSFRC_RLDCSF_SHIFT);

  am67_epwm_putreg16(base, AM67_EPWM_AQSFRC_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_park_outputs
 *
 * Description:
 *   Park BOTH pins low via continuous software force.  One composed
 *   write: the stop path owns every field of AQCSFRC (both targets are
 *   known), so both pins park in the same instruction with no window
 *   where one waves on alone.  The force is a mask in front of the AQ
 *   output latch, not a write to it; releasing it exposes whatever the
 *   latch last held.
 *
 ****************************************************************************/

static void am67_epwm_park_outputs(uint32_t base)
{
  uint16_t regval;

  am67_epwm_rldcsf_immediate(base);

  regval  = (AM67_EPWM_CSFA_FORCE_LOW << AM67_EPWM_AQCSFRC_CSFA_SHIFT);
  regval |= (AM67_EPWM_CSFA_FORCE_LOW << AM67_EPWM_AQCSFRC_CSFB_SHIFT);

  am67_epwm_putreg16(base, AM67_EPWM_AQCSFRC_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_force_release
 *
 * Description:
 *   Release ONE channel's continuous software force so that pin follows
 *   the AQ output again.  Read-modify-write of that channel's 2-bit
 *   field only: CSFA and CSFB share this register, and a full write
 *   here would release the neighbor's park as a side effect.  Called
 *   only after that channel is wired and the counter runs: the AQ latch
 *   is then live wave state, so the release exposes the true waveform
 *   and not a stale level.
 *
 ****************************************************************************/

static void am67_epwm_force_release(uint32_t base, int8_t channel)
{
  uint16_t regval;

  am67_epwm_rldcsf_immediate(base);

  regval = am67_epwm_getreg16(base, AM67_EPWM_AQCSFRC_OFFSET);

  if (channel == 1)
    {
      regval &= ~AM67_EPWM_AQCSFRC_CSFA_MASK;
      regval |= (AM67_EPWM_CSFA_FORCE_DISABLE <<
                 AM67_EPWM_AQCSFRC_CSFA_SHIFT);
    }
  else
    {
      regval &= ~AM67_EPWM_AQCSFRC_CSFB_MASK;
      regval |= (AM67_EPWM_CSFA_FORCE_DISABLE <<
                 AM67_EPWM_AQCSFRC_CSFB_SHIFT);
    }

  am67_epwm_putreg16(base, AM67_EPWM_AQCSFRC_OFFSET, regval);
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

static void am67_epwm_set_tbctl(uint32_t base)
{
  uint16_t regval = (AM67_EPWM_TBCTL_CTRMODE_STOP_FREEZE <<
                     AM67_EPWM_TBCTL_CTRMODE_SHIFT);

  regval |= AM67_EPWM_TBCTL_PRDLD_IMMEDIATE;
  regval |= (3u << AM67_EPWM_TBCTL_SYNCOSEL_SHIFT);  /* 3 = SYNCO off */

  am67_epwm_putreg16(base, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_set_cmpctl
 *
 * Description:
 *   Shadow CMPA/CMPB (SHDWxMODE=0) and load them at CTR=0 (LOADxMODE=0),
 *   the start of each pulse.
 *
 *   An immediate write can land after the counter has passed the new
 *   compare but not the old one. The CLEAR for that period is then
 *   missed, the output stays high into the next period, and the ESC sees
 *   one pulse longer than the period (> 2.5 ms at 400 Hz) when the
 *   throttle is lowered. With the load at CTR=0 each period uses one
 *   compare value from start to end. A new duty takes effect at the next
 *   period, at most one period late.
 *
 ****************************************************************************/

static void am67_epwm_set_cmpctl(uint32_t base)
{
  /* SHDWAMODE = SHDWBMODE = 0 (shadowed), LOADAMODE = LOADBMODE = 0
   * (load on CTR = 0).
   */

  am67_epwm_putreg16(base, AM67_EPWM_CMPCTL_OFFSET, 0u);
}

/****************************************************************************
 * Name: am67_epwm_clear_cmpctl
 *
 * Description:
 *   Return CMPCTL to its reset state (all fields zero).
 *
 ****************************************************************************/

static void am67_epwm_clear_cmpctl(uint32_t base)
{
  am67_epwm_putreg16(base, AM67_EPWM_CMPCTL_OFFSET, 0u);
}

/****************************************************************************
 * Name: am67_epwm_tbctl_ctrmode_freeze
 *
 * Description:
 *   Freeze the time-base counter (CTRMODE = stop-freeze).  Freeze holds
 *   the current count and pin level; it does not clear them.
 *
 ****************************************************************************/

static void am67_epwm_tbctl_ctrmode_freeze(uint32_t base)
{
  uint16_t regval = am67_epwm_getreg16(base, AM67_EPWM_TBCTL_OFFSET);

  regval &= ~AM67_EPWM_TBCTL_CTRMODE_MASK;
  regval |= (AM67_EPWM_TBCTL_CTRMODE_STOP_FREEZE <<
             AM67_EPWM_TBCTL_CTRMODE_SHIFT);

  am67_epwm_putreg16(base, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_tbctl_ctrmode_up
 *
 * Description:
 *   Ignition: flip CTRMODE from freeze to up-count with everything else
 *   already configured.  Clear-then-set on the live register.
 *
 ****************************************************************************/

static void am67_epwm_tbctl_ctrmode_up(uint32_t base)
{
  uint16_t regval = am67_epwm_getreg16(base, AM67_EPWM_TBCTL_OFFSET);

  regval &= ~AM67_EPWM_TBCTL_CTRMODE_MASK;
  regval |= (AM67_EPWM_TBCTL_CTRMODE_UP << AM67_EPWM_TBCTL_CTRMODE_SHIFT);

  am67_epwm_putreg16(base, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_reset_tbcnt
 *
 * Description:
 *   Zero the time-base counter (safe while frozen; clears stale count).
 *
 ****************************************************************************/

static void am67_epwm_reset_tbcnt(uint32_t base)
{
  am67_epwm_putreg16(base, AM67_EPWM_TBCNT_OFFSET, 0u);
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

static void am67_epwm_set_clock_values(uint32_t base, uint16_t hsp,
                                       uint16_t clk, uint16_t tbprd)
{
  uint16_t regval;

  am67_epwm_putreg16(base, AM67_EPWM_TBPRD_OFFSET, tbprd);

  regval = am67_epwm_getreg16(base, AM67_EPWM_TBCTL_OFFSET);

  regval &= ~AM67_EPWM_TBCTL_HSPCLKDIV_MASK;
  regval |= (hsp / 2) << AM67_EPWM_TBCTL_HSPCLKDIV_SHIFT;

  regval &= ~AM67_EPWM_TBCTL_CLKDIV_MASK;
  regval |= am67_epwm_loop_log2(clk) << AM67_EPWM_TBCTL_CLKDIV_SHIFT;

  am67_epwm_putreg16(base, AM67_EPWM_TBCTL_OFFSET, regval);
}

/****************************************************************************
 * Name: am67_epwm_set_duty
 *
 * Description:
 *   Convert the ub16 duty fraction to compare ticks and write it to the
 *   given channel's compare register (CMPA for channel 1, CMPB for
 *   channel 2).  The write goes to the shadow register and becomes active
 *   at the next CTR=0 (see am67_epwm_set_cmpctl).  duty = 0 gives exact
 *   0% (compare outranks zero in AQ priority); exact 100% is unreachable
 *   by the ub16 format itself (max 65535/65536).
 *
 ****************************************************************************/

static void am67_epwm_set_duty(struct pwm_lowerhalf_s *dev, int8_t channel,
                               ub16_t duty)
{
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;
  uint32_t offset = (channel == 1) ? AM67_EPWM_CMPA_OFFSET
                                   : AM67_EPWM_CMPB_OFFSET;
  uint16_t cmp = (uint16_t)((duty * (priv->tbprd + 1u)) >> 16);

  am67_epwm_putreg16(priv->base, offset, cmp);
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
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;
  int ret;

  ret = am67_epwm_enable_clock(priv->clken_mask);
  if (ret < 0)
    {
      return ret;
    }

  ret = am67_epwm_check_pid(priv->base);
  if (ret < 0)
    {
      return ret;
    }

  /* The trip-zone registers ignore writes until CTRL_MMR EPWMn_CTRL
   * EALLOW is set. Leave it set so the emergency stop can force a trip.
   */

  am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE,
                   AM67_CTRL_MMR_EPWM_CTRL(priv->pinmux_id),
                   am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                                    AM67_CTRL_MMR_EPWM_CTRL(priv->pinmux_id)) |
                   AM67_CTRL_MMR_EPWM_CTRL_EALLOW);

  /* Force-low is the trip action. Clear a trip left latched by a crashed
   * image: the module keeps its state across an R5F reload, and a latched
   * one-shot trip would hold every output low.
   */

  am67_epwm_putreg16(priv->base, AM67_EPWM_TZCTL_OFFSET,
                     AM67_EPWM_TZCTL_TZA_LOW | AM67_EPWM_TZCTL_TZB_LOW);
  am67_epwm_putreg16(priv->base, AM67_EPWM_TZCLR_OFFSET,
                     AM67_EPWM_TZ_OST | AM67_EPWM_TZ_CBC | AM67_EPWM_TZ_INT);

  if (am67_epwm_getreg16(priv->base, AM67_EPWM_TZCTL_OFFSET) !=
      (AM67_EPWM_TZCTL_TZA_LOW | AM67_EPWM_TZCTL_TZB_LOW))
    {
      pwmerr("ERROR: EPWM%u trip-zone not writable\n", priv->pinmux_id);
      return -EIO;
    }

  /* No event-trigger interrupt: a DShot image may have left it enabled,
   * and the module keeps its state across an R5F reload.
   */

  am67_epwm_putreg16(priv->base, AM67_EPWM_ETSEL_OFFSET, 0);
  am67_epwm_putreg16(priv->base, AM67_EPWM_ETCLR_OFFSET, AM67_EPWM_ET_INT);

  am67_epwm_pinmux_init(priv->pinmux_id);
  am67_epwm_set_cmpctl(priv->base);
  am67_epwm_set_tbctl(priv->base);

  priv->live = true;
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
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;

  am67_epwm_stop(dev);
  am67_epwm_clear_aqctl(priv->base, 1);
  am67_epwm_clear_aqctl(priv->base, 2);
  am67_epwm_clear_cmpctl(priv->base);

  return am67_epwm_disable_clock(priv->clken_mask);
}

/****************************************************************************
 * Name: am67_epwm_start
 *
 * Description:
 *   Start (or update) the pulsed output(s).  One unified path; the
 *   module-level work is conditional on a frequency change:
 *
 *   VALIDATE: scan the channel array (channel 1 = output A, 2 = B;
 *     0 skips a slot, -1 ends the array; anything else, a duplicate
 *     channel, or an active duty above ub16 is rejected).  Any failure
 *     returns here with neither hardware nor cached state touched, so
 *     a running wave survives a bad request unchanged.  A request with
 *     no active channels is a legal no-op.
 *   ACT (module, only if the frequency changed): solve dividers, stop
 *     first (parks both pins and clears the active set, so every
 *     requested channel re-wires as a joiner below), program the
 *     timebase.
 *   ACT (per channel, always): write the compare; a channel not in the
 *     active set is a joiner and gets its AQ wiring here.  While
 *     AQCTLx is still reset (all zero) the compare write is inert, so
 *     configuring a joiner cannot glitch a live neighbor.
 *   IGNITE: only a frequency change needs it; on a pure join the
 *     counter is already running.  Joiners' forces are released after
 *     ignition (the AQ latch is then live wave state), one field-RMW
 *     each so an already-running neighbor's force bits are untouched.
 *   COMMIT: frequency/tbprd land just before the module writes because
 *     set_duty derives the compare from priv->tbprd; the active set is
 *     updated last, after the writes that cannot fail.
 *
 *   The first period after (re)ignition runs with the previous active
 *   compare (0 on a fresh start = one silent period): accepted,
 *   safe-low.
 *
 * Returned Value:
 *   Zero (OK) on success; -EINVAL for a bad channel array or duty;
 *   -ERANGE if the frequency cannot be produced.
 *
 ****************************************************************************/

static int am67_epwm_start(struct pwm_lowerhalf_s *dev,
                           const struct pwm_info_s *info)
{
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;
  uint8_t requested = 0;
  uint8_t joiners = 0;
  uint16_t hsp = 0;
  uint16_t clk = 0;
  uint16_t tbprd = 0;
  bool freq_changed;
  int8_t ch;
  int ret;
  int i;

  /* VALIDATE: one pass over the array, no writes.  The requested mask
   * doubles as the duplicate detector: a channel's bit already set
   * means the caller named it twice with (potentially) two different
   * duties, and guessing which one was meant is not this driver's job.
   */

  for (i = 0; i < CONFIG_PWM_NCHANNELS; i++)
    {
      ch = info->channels[i].channel;

      if (ch == -1)
        {
          break;
        }

      if (ch == 0)
        {
          continue;
        }

      if (ch != 1 && ch != 2)
        {
          pwmerr("ERROR: No such channel: %d\n", ch);
          return -EINVAL;
        }

      if ((requested & CH_BIT(ch)) != 0u)
        {
          pwmerr("ERROR: Channel %d requested twice\n", ch);
          return -EINVAL;
        }

      if (info->channels[i].duty > 0xffff)
        {
          pwmerr("ERROR: Duty out of range on channel %d\n", ch);
          return -EINVAL;
        }

      requested |= CH_BIT(ch);
    }

  if (requested == 0u)
    {
      return OK;
    }

  freq_changed = (priv->frequency != info->frequency);

  if (freq_changed)
    {
      ret = am67_epwm_calculate_clock_values(info->frequency, &hsp, &clk,
                                             &tbprd);
      if (ret < 0)
        {
          pwmerr("ERROR: Cannot produce %" PRIu32 " Hz, keeping %" PRIu32
                 " Hz\n", info->frequency, priv->frequency);
          return ret;
        }

      /* stop() parks the pins and clears priv->frequency. Do that
       * before publishing the new rate: publishing first made the next
       * start() see frequency 0 and park the outputs on every update.
       */

      am67_epwm_stop(dev);

      /* set_duty derives the compare from priv->tbprd. */
      priv->frequency = info->frequency;
      priv->tbprd     = tbprd;

      am67_epwm_set_clock_values(priv->base, hsp, clk, tbprd);
      am67_epwm_reset_tbcnt(priv->base);
    }

  for (i = 0; i < CONFIG_PWM_NCHANNELS; i++)
    {
      ch = info->channels[i].channel;

      if (ch == -1)
        {
          break;
        }

      if (ch == 0)
        {
          continue;
        }

      am67_epwm_set_duty(dev, ch, info->channels[i].duty);

      /* A channel not yet in the active set is a joiner: wire its AQ
       * actions here; its force is released after ignition below.
       */

      if ((priv->active_channels & CH_BIT(ch)) == 0u)
        {
          am67_epwm_config_aqctl(priv->base, ch);
          joiners |= CH_BIT(ch);
        }
    }

  if (freq_changed)
    {
      am67_epwm_tbctl_ctrmode_up(priv->base);
    }

  /* Force release LAST: post-ignition the AQ latch carries live wave
   * state, not a fossil.  Field-RMW per joiner - a channel that was
   * already waving keeps its (released) force bits untouched, and a
   * channel that stays parked keeps its park.
   */

  if ((joiners & CH_A_ACTIVE) != 0u)
    {
      am67_epwm_force_release(priv->base, 1);
    }

  if ((joiners & CH_B_ACTIVE) != 0u)
    {
      am67_epwm_force_release(priv->base, 2);
    }

  priv->active_channels |= requested;

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_stop
 *
 * Description:
 *   Park both outputs: force the pins low (RLDCSF first, one composed
 *   write for both channels - no window where one waves on alone),
 *   then freeze the counter.  The order is load-bearing: the force
 *   masks the pins before the freeze fossilizes the AQ latches at
 *   arbitrary levels.  Both caches are invalidated: frequency = 0 (no
 *   wave) so a same-frequency restart cannot skip the module block on
 *   a frozen timebase, and the active set is emptied so every channel
 *   of the next start re-wires and re-releases as a joiner.  The books
 *   go dark here because this is where reality does.
 *
 * Returned Value:
 *   Zero (OK) always.
 *
 ****************************************************************************/

static int am67_epwm_stop(struct pwm_lowerhalf_s *dev)
{
  struct am67_epwm_s *priv = (struct am67_epwm_s *)dev;

  am67_epwm_park_outputs(priv->base);
  am67_epwm_tbctl_ctrmode_freeze(priv->base);

  priv->frequency       = 0;
  priv->active_channels = 0;

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
 *   pwm - PWM instance number: 0 (EPWM0) or 1 (EPWM1).  EPWM2 is the
 *   board cooling fan and is deliberately not supported.
 *
 * Returned Value:
 *   Pointer to the lower-half driver on success; NULL on an unsupported
 *   or unconfigured instance number.
 *
 ****************************************************************************/

struct pwm_lowerhalf_s *am67_epwminitialize(int pwm)
{
  switch (pwm)
    {
#ifdef CONFIG_AM67_EPWM0
      case 0:
        pwminfo("Initialize EPWM%d\n", pwm);
        return (struct pwm_lowerhalf_s *)&g_am67_epwm0;
#endif

#ifdef CONFIG_AM67_EPWM1
      case 1:
        pwminfo("Initialize EPWM%d\n", pwm);
        return (struct pwm_lowerhalf_s *)&g_am67_epwm1;
#endif

      default:
        pwmerr("ERROR: No such PWM instance: %d\n", pwm);
        return NULL;
    }
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
  static struct notifier_block nb =
  {
    .notifier_call = am67_epwm_panic,
  };

  static bool registered;

  if (!registered)
    {
      registered = true;
      panic_notifier_chain_register(&nb);
    }

  return am67_epwm_enable_register_write();
}

/****************************************************************************
 * Name: am67_epwm_dshot_setup
 *
 * Description:
 *   Take EPWM0 and EPWM1 (all four motor pins) for DShot, driven bit by
 *   bit from am67_dshot.c.  Each counter period is one DShot bit: the pin
 *   goes high at CTR = 0 and low at the compare, and the compares are
 *   shadowed (loaded at CTR = 0), so every edge is placed by hardware; the
 *   software only has to write the next bit within one bit period.  EPWM1
 *   follows EPWM0 through the hardwired SYNCOUT -> SYNCIN link, so the four
 *   outputs share one time base and one interrupt (EPWM0 ET at CTR = 0).
 *   setup() still runs first: power, pinmux, trip zone and EALLOW, so the
 *   emergency stop keeps working.  The pins are held low by the continuous
 *   software force, which am67_dshot.c releases per bit; it loads at
 *   CTR = PRD, where no action is programmed.  Compares stay non-zero: a
 *   compare loaded at CTR = 0 does not act at that same CTR = 0.  Changing
 *   the protocol of a running module needs a reboot (PWM_MAIN_TIMx).
 *
 * Input Parameters:
 *   tbprd - counter period minus one, in 250 MHz TBCLK ticks
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

/****************************************************************************
 * Name: am67_epwm_tbclk_guard
 *
 * Description:
 *   The time-base clock gates sit in a CTRL_MMR register, outside the
 *   DM's per-host device accounting, so another host can gate them off
 *   under a running core.  Linux does at boot ("clk: Disabling unused
 *   clocks") for every gate it has no consumer of: with this core started
 *   before Linux that stops the motor outputs.  Turn back on any gate this
 *   core enabled that is off now.  One register read when nothing changed;
 *   meant to run every output cycle, from thread context.
 *
 * Returned Value:
 *   How many times a gate had to be turned back on since boot.
 *
 ****************************************************************************/

uint32_t am67_epwm_tbclk_guard(void)
{
  uint32_t owned = g_tbclk_owned;
  uint32_t regval;
  uint32_t n;

  if (owned == 0u)
    {
      return g_tbclk_restores;
    }

  regval = am67_epwm_getreg(AM67_MAIN_CTRL_MMR_BASE,
                            AM67_CTRL_MMR_EPWM_TB_CLKEN);
  if ((regval & owned) == owned)
    {
      return g_tbclk_restores;
    }

  /* The partition may have been locked again, too */

  am67_epwm_enable_register_write();
  am67_epwm_putreg(AM67_MAIN_CTRL_MMR_BASE, AM67_CTRL_MMR_EPWM_TB_CLKEN,
                   regval | owned);

  n = ++g_tbclk_restores;
  if ((n & (n - 1u)) == 0u)
    {
      syslog(LOG_WARNING, "[pwm] EPWM TB_CLKEN 0x%08" PRIx32 ": gate "
             "turned off by another host, restored (%" PRIu32 ")\n",
             regval, n);
    }

  return n;
}

int am67_epwm_dshot_setup(uint16_t tbprd)
{
  struct am67_epwm_s *const epwm[2] =
  {
    &g_am67_epwm0, &g_am67_epwm1
  };

  int ret;
  int n;

  for (n = 0; n < 2; n++)
    {
      struct am67_epwm_s *priv = epwm[n];
      uint32_t base = priv->base;
      uint16_t tbctl;

      if (!priv->live)
        {
          ret = am67_epwm_setup((struct pwm_lowerhalf_s *)priv);
          if (ret < 0)
            {
              return ret;
            }
        }

      /* Hold both pins low while the module is reconfigured */

      am67_epwm_park_outputs(base);

      /* Frozen, TBCLK = 250 MHz (no prescale), period shadowed.  EPWM0
       * emits SYNCOUT at CTR = 0; EPWM1 reloads its counter from it.
       */

      tbctl = AM67_EPWM_TBCTL_CTRMODE_STOP_FREEZE <<
              AM67_EPWM_TBCTL_CTRMODE_SHIFT;
      tbctl |= (n == 0 ? 1u : 3u) << AM67_EPWM_TBCTL_SYNCOSEL_SHIFT;
      if (n == 1)
        {
          tbctl |= AM67_EPWM_TBCTL_PHSEN_MASK;
        }

      am67_epwm_putreg16(base, AM67_EPWM_TBCTL_OFFSET, tbctl);
      am67_epwm_putreg16(base, AM67_EPWM_TBPRD_OFFSET, tbprd);
      am67_epwm_putreg16(base, AM67_EPWM_TBPHS_OFFSET, 0);
      am67_epwm_putreg16(base, AM67_EPWM_TBCNT_OFFSET, 0);
      am67_epwm_putreg16(base, AM67_EPWM_CMPCTL_OFFSET, 0);
      am67_epwm_putreg16(base, AM67_EPWM_CMPA_OFFSET, tbprd / 2);
      am67_epwm_putreg16(base, AM67_EPWM_CMPB_OFFSET, tbprd / 2);

      /* High at CTR = 0, low at the compare, nothing at PRD */

      am67_epwm_putreg16(base, AM67_EPWM_AQCTLA_OFFSET,
                         (AM67_EPWM_AQ_SET << 0) | (AM67_EPWM_AQ_CLEAR << 4));
      am67_epwm_putreg16(base, AM67_EPWM_AQCTLB_OFFSET,
                         (AM67_EPWM_AQ_SET << 0) | (AM67_EPWM_AQ_CLEAR << 8));

      am67_epwm_putreg16(base, AM67_EPWM_ETSEL_OFFSET,
                         AM67_EPWM_ETSEL_INTSEL_ZERO);
      am67_epwm_putreg16(base, AM67_EPWM_ETPS_OFFSET,
                         AM67_EPWM_ETPS_INTPRD_1);
      am67_epwm_putreg16(base, AM67_EPWM_ETCLR_OFFSET, AM67_EPWM_ET_INT);

      priv->frequency = 0;
      priv->tbprd = tbprd;
      priv->active_channels = CH_A_ACTIVE | CH_B_ACTIVE;
    }

  /* Start EPWM1 first so the first EPWM0 SYNCOUT aligns it */

  for (n = 1; n >= 0; n--)
    {
      uint32_t base = epwm[n]->base;

      am67_epwm_putreg16(base, AM67_EPWM_TBCTL_OFFSET,
                         (am67_epwm_getreg16(base, AM67_EPWM_TBCTL_OFFSET) &
                          ~AM67_EPWM_TBCTL_CTRMODE_MASK) |
                         (AM67_EPWM_TBCTL_CTRMODE_UP <<
                          AM67_EPWM_TBCTL_CTRMODE_SHIFT));
    }

  /* From now on the force loads at PRD; it stays low until a frame */

  for (n = 0; n < 2; n++)
    {
      am67_epwm_putreg16(epwm[n]->base, AM67_EPWM_AQSFRC_OFFSET,
                         AM67_EPWM_AQSFRC_RLDCSF_PERIOD <<
                         AM67_EPWM_AQSFRC_RLDCSF_SHIFT);
      am67_epwm_putreg16(epwm[n]->base, AM67_EPWM_AQCSFRC_OFFSET,
                         (AM67_EPWM_CSFA_FORCE_LOW << 0) |
                         (AM67_EPWM_CSFA_FORCE_LOW << 2));
    }

  return OK;
}

/****************************************************************************
 * Name: am67_epwm_emergency_stop
 *
 * Description:
 *   Drive every EPWM output that has been set up low at once, with a
 *   one-shot trip. Register writes only: safe from an assert, an
 *   exception handler or an ISR. The trip stays latched until the next
 *   setup().
 *
 ****************************************************************************/

void am67_epwm_emergency_stop(void)
{
#ifdef CONFIG_AM67_EPWM0
  if (g_am67_epwm0.live)
    {
      am67_epwm_putreg16(g_am67_epwm0.base, AM67_EPWM_TZFRC_OFFSET,
                         AM67_EPWM_TZ_OST);
    }
#endif

#ifdef CONFIG_AM67_EPWM1
  if (g_am67_epwm1.live)
    {
      am67_epwm_putreg16(g_am67_epwm1.base, AM67_EPWM_TZFRC_OFFSET,
                         AM67_EPWM_TZ_OST);
    }
#endif
}

/* Any assertion or CPU exception: a flight task may be dead, so do not
 * leave the motors on their last command. The ESCs see the signal go
 * low and stop.
 */

static int am67_epwm_panic(FAR struct notifier_block *nb,
                           unsigned long action, FAR void *data)
{
  am67_epwm_emergency_stop();
  return 0;
}

#endif /* CONFIG_AM67_EPWM0 || CONFIG_AM67_EPWM1 */
