/****************************************************************************
 * arch/arm/src/am67/am67_dshot.c
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

/* DShot on the AM67 EPWM motor outputs.
 *
 * The SoC has no DMA request from EPWM/eCAP and no PRU, so the timer + DMA
 * scheme other flight controllers use is not available.  Instead each
 * EPWM counter period is one DShot bit (am67_epwm_dshot_setup()): the pin
 * rises at CTR = 0 and falls at the shadowed compare, so the edges come
 * from hardware.  Between frames the continuous software force holds the
 * pins low.  A FIQ at every CTR = 0 (EPWM0 event trigger, VIM 36) writes
 * the compares and the force of the following bit.  FIQs are masked
 * neither by NuttX critical sections (up_irq_save() only sets CPSR.I) nor,
 * with ARMV7R_FIQ_NMI, while an interrupt is handled, so the only timing
 * requirement is that the handler runs within one bit period.
 *
 * The handler (am67_dshot_fiq.S) is patched into the FIQ vector ahead of
 * the generic NuttX FIQ entry and runs from ATCM on the banked FIQ
 * registers, without a stack.  Any other FIQ (the RTI8 watchdog) goes on to
 * the generic entry.  A frame is 18 rows: a sync row that holds the pins
 * low, 16 bits and a final row that holds them low again.  The trigger
 * only enables the interrupt.  With the event counter saturated the first
 * FIQ can come at once, anywhere in a period; it writes the sync row,
 * which changes nothing, and bit 0 is written from the next CTR = 0 on.
 * A row that is still being written when the next CTR = 0 passes drops
 * the frame (am67_dshot_fiq.S).
 *
 * While a frame is on the wire the core runs nothing from DDR: the trigger
 * waits for it in ATCM with IRQs masked (dshot_send()).  An R5F access to
 * DDR can stall for about 26 us (measured, board idle: the DDR stays busy
 * with its own upkeep; no stall while the A53s load the DDR), and no FIQ is
 * taken during the stall, so a frame sent next to running code repeated
 * bits (about 8 frames per minute).  The wait costs one frame time per
 * frame (30 us at DShot600) and delays IRQs by as much.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "arm_internal.h"
#include "sctlr.h"
#include "am67_dshot.h"
#include "am67_pwm.h"

#ifdef CONFIG_AM67_DSHOT

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define EPWM0_BASE          0x23000000u
#define EPWM1_BASE          0x23010000u
#define EPWM_TBCLK_HZ       250000000u
#define EPWM_ETSEL          0x032u
#define EPWM_ETCLR          0x038u
#define EPWM_ETSEL_INTEN    (1u << 3)
#define EPWM_AQCSFRC        0x01cu

#define DSHOT_ROWS          18          /* sync + 16 bits + trailing low */

/* AQCSFRC per module: CSFA bits 1:0, CSFB bits 3:2, 1 = force low,
 * 0 = follow the action qualifier.
 */

#define CSF_LOW_A           (1u << 0)
#define CSF_LOW_B           (1u << 2)
#define CSF_IDLE            ((CSF_LOW_A | CSF_LOW_B) | \
                             ((CSF_LOW_A | CSF_LOW_B) << 16))
#define DSHOT_IRQ           36          /* EPWM0 etint -> R5FSS0 VIM 36 */

#define VIM_BASE            0x2fff0000u
#define VIM_STS(j)          (VIM_BASE + 0x404u + (((j) >> 5) * 0x20u))
#define VIM_INT_MAP(j)      (VIM_BASE + 0x418u + (((j) >> 5) * 0x20u))
#define VIM_INT_TYPE(j)     (VIM_BASE + 0x41cu + (((j) >> 5) * 0x20u))
#define VIM_BIT(j)          (1u << ((j) & 0x1fu))

/* ARMv7-R vector table: the FIQ entry is "ldr pc, [pc, #24]" at 0x1c,
 * loading its target from the literal at 0x3c (ATCM).
 */

#define FIQ_VECTOR_LITERAL  0x0000003cu

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Shared with am67_dshot_fiq.S: keep the offsets in sync */

struct am67_dshot_fiq_s
{
  uint32_t idx;         /*  0: next row to write */
  uint32_t nrows;       /*  4: rows per frame */
  uint32_t table;       /*  8: address of the row table */
  uint32_t epwm0;       /* 12 */
  uint32_t epwm1;       /* 16 */
  uint32_t generic;     /* 20: previous FIQ vector target */
  uint32_t busy;        /* 24: frame in progress */
  uint32_t frames;      /* 28: frames completed */
  uint32_t late;        /* 32: frames dropped, row written too late */
  uint32_t maxcnt;      /* 36: highest TBCNT at FIQ exit */
  uint32_t entry;       /* 40: scratch, PMCCNTR at FIQ entry */
  uint32_t maxcyc;      /* 44: most CPU cycles spent in the FIQ */
  uint32_t bit0;        /* 48: scratch, PMCCNTR at the bit 0 FIQ */
  uint32_t framecyc;    /* 52: cycles from the bit 0 FIQ to the last */
  uint32_t late_row;    /* 56: last late FIQ: rows written */
  uint32_t late_exit;   /* 60: its TBCNT at exit */
  uint32_t late_spent;  /* 64: its ticks in the FIQ (skipped: since anchor) */
  uint32_t tbprd;       /* 68: TBPRD, the force loads at this count */
  uint32_t exitcyc;     /* 72: scratch, PMCCNTR at FIQ exit */
  uint32_t anchorcyc;   /* 76: PMCCNTR at the bit 0 FIQ exit */
  uint32_t anchorcnt;   /* 80: TBCNT at the bit 0 FIQ exit */
  uint32_t limit;       /* 84: ticks from the anchor CTR = 0: skipped */
  uint32_t spent;       /* 88: scratch, cycles in this FIQ */
  uint32_t skipped;     /* 92: frames dropped, a CTR = 0 raised no FIQ */
  uint32_t worstpc;     /* 96: lr_fiq at the worst exit count */
  uint32_t slow;        /* 100: rows written past half a period */
  uint32_t skippc;      /* 104: lr_fiq of the last skipped FIQ */
  uint32_t maxtotal;    /* 108: worst whole FIQ, entry to return, cycles */
  uint32_t maxgap;      /* 112: worst gap between FIQ entries in a frame */
  uint32_t preventry;   /* 116: scratch, PMCCNTR at the previous entry */
  uint32_t gappc;       /* 120: lr_fiq at the worst gap */
  uint32_t gapn;        /* 124: gaps over two periods, ring index */
  uint32_t gapring[16][2]; /* 128: last such gaps: cycles, lr_fiq */
};

/****************************************************************************
 * Public Data (used by am67_dshot_fiq.S)
 ****************************************************************************/

/* ATCM: no cache, no wait states; the FIQ reads both on every bit */

locate_data(".am67_fastdata") volatile struct am67_dshot_fiq_s g_am67_dshot;
locate_data(".am67_fastdata") uint32_t g_am67_dshot_rows[DSHOT_ROWS][3];

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint16_t g_packet[AM67_DSHOT_NCHANNELS];
static uint32_t g_mask;
static uint16_t g_cmp1;
static uint16_t g_cmp0;
static uint16_t g_ticks;
static uint32_t g_csf_bits;
static uint32_t g_timeout_cycles;
static uint32_t g_timeouts;
static bool g_armed;
static bool g_ready;
static uint32_t g_overruns;

/****************************************************************************
 * External Function Prototypes
 ****************************************************************************/

void am67_dshot_fiq(void);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void dshot_install_fiq(void)
{
  irqstate_t flags;
  uint32_t state = (uint32_t)(uintptr_t)&g_am67_dshot;

  flags = up_irq_save();
  __asm__ __volatile__ ("cpsid f" ::: "memory");

  g_am67_dshot.generic = getreg32(FIQ_VECTOR_LITERAL);

  /* r8_fiq holds the state pointer for the handler */

  __asm__ __volatile__
    (
      "mrs   r1, cpsr\n\t"
      "cps   #0x11\n\t"
      "mov   r8, %0\n\t"
      "msr   cpsr_c, r1\n\t"
      :
      : "r" (state)
      : "r1", "memory"
    );

  putreg32((uint32_t)(uintptr_t)am67_dshot_fiq, FIQ_VECTOR_LITERAL);
  UP_DSB();
  __asm__ __volatile__ ("isb" ::: "memory");

  /* EPWM0 event trigger as a pulse FIQ.  Equal VIM priority: the RTI8
   * watchdog (input 30) is still served first.
   */

  putreg32(VIM_BIT(DSHOT_IRQ), VIM_STS(DSHOT_IRQ));
  modifyreg32(VIM_INT_TYPE(DSHOT_IRQ), 0, VIM_BIT(DSHOT_IRQ));
  modifyreg32(VIM_INT_MAP(DSHOT_IRQ), 0, VIM_BIT(DSHOT_IRQ));
  up_enable_irq(DSHOT_IRQ);

  __asm__ __volatile__ ("cpsie f" ::: "memory");
  up_irq_restore(flags);
}

/****************************************************************************
 * Name: dshot_send
 *
 * Description:
 *   Start the staged frame and wait until the FIQ has written its last
 *   row.  Runs from ATCM with IRQs masked and touches only ATCM and
 *   peripheral registers, so nothing can stall the core on DDR while the
 *   FIQ engine needs it.  No calls: everything here must stay in ATCM.
 *
 ****************************************************************************/

locate_code(".am67_fastcode") noinline_function
static int dshot_send(uint32_t timeout_cycles)
{
  uint32_t flags;
  uint32_t start;
  uint32_t now;
  int ret = OK;

  __asm__ __volatile__ ("mrs %0, cpsr\n\tcpsid i" : "=r" (flags) :: "memory");
  __asm__ __volatile__ ("mrc p15, 0, %0, c9, c13, 0" : "=r" (start));

  putreg16(1, EPWM0_BASE + EPWM_ETCLR);
  putreg16(getreg16(EPWM0_BASE + EPWM_ETSEL) | EPWM_ETSEL_INTEN,
           EPWM0_BASE + EPWM_ETSEL);

  while (g_am67_dshot.busy)
    {
      __asm__ __volatile__ ("mrc p15, 0, %0, c9, c13, 0" : "=r" (now));
      if (now - start > timeout_cycles)
        {
          /* No FIQ: stop the engine, pins low from the next PRD on */

          putreg16(getreg16(EPWM0_BASE + EPWM_ETSEL) & ~EPWM_ETSEL_INTEN,
                   EPWM0_BASE + EPWM_ETSEL);
          putreg16(CSF_LOW_A | CSF_LOW_B, EPWM0_BASE + EPWM_AQCSFRC);
          putreg16(CSF_LOW_A | CSF_LOW_B, EPWM1_BASE + EPWM_AQCSFRC);
          g_am67_dshot.busy = 0;
          ret = -ETIMEDOUT;
          break;
        }
    }

  __asm__ __volatile__ ("msr cpsr_c, %0" :: "r" (flags) : "memory");
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int am67_dshot_init(uint32_t bitrate, uint32_t channel_mask)
{
  uint32_t ticks;
  int ret;

  if (bitrate != 150000 && bitrate != 300000 && bitrate != 600000)
    {
      return -EINVAL;
    }

  ticks = (EPWM_TBCLK_HZ + bitrate / 2) / bitrate;
  ret = am67_epwm_dshot_setup((uint16_t)(ticks - 1));
  if (ret < 0)
    {
      return ret;
    }

  /* DShot: a 1 is high for 3/4 of the bit, a 0 for 3/8 */

  g_ticks = (uint16_t)ticks;

  /* Four frames: 18 rows of ticks * 3.2 CPU cycles (800 / 250 MHz) */

  g_timeout_cycles = ticks * DSHOT_ROWS * 16 / 5 * 4;
  g_cmp1 = (uint16_t)(ticks * 3 / 4);
  g_cmp0 = (uint16_t)(ticks * 3 / 8);
  g_mask = channel_mask & ((1u << AM67_DSHOT_NCHANNELS) - 1u);

  /* During the bits only the channels in use follow the compares */

  g_csf_bits = CSF_IDLE;
  g_csf_bits &= (g_mask & 1u) ? ~CSF_LOW_A : ~0u;
  g_csf_bits &= (g_mask & 2u) ? ~CSF_LOW_B : ~0u;
  g_csf_bits &= (g_mask & 4u) ? ~(CSF_LOW_A << 16) : ~0u;
  g_csf_bits &= (g_mask & 8u) ? ~(CSF_LOW_B << 16) : ~0u;

  g_am67_dshot.idx = 0;
  g_am67_dshot.nrows = DSHOT_ROWS;
  g_am67_dshot.table = (uint32_t)(uintptr_t)g_am67_dshot_rows;
  g_am67_dshot.epwm0 = EPWM0_BASE;
  g_am67_dshot.epwm1 = EPWM1_BASE;
  g_am67_dshot.busy = 0;
  g_am67_dshot.tbprd = ticks - 1;
  g_am67_dshot.maxcnt = 0;
  g_am67_dshot.maxcyc = 0;
  g_am67_dshot.worstpc = 0;

  /* The FIQ times itself on the PMU cycle counter (idempotent, same setup
   * as up_perf_init()).
   */

  cp15_pmu_pmcr(PMCR_E);
  cp15_pmu_cesr(PMCESR_CCES);

  if (!g_ready)
    {
      dshot_install_fiq();
      g_ready = true;
    }

  return (int)g_mask;
}

void am67_dshot_set(unsigned channel, uint16_t value, bool telemetry)
{
  uint16_t packet;
  uint16_t crc;

  if (channel >= AM67_DSHOT_NCHANNELS)
    {
      return;
    }

  packet = (uint16_t)(((value & 0x07ffu) << 1) | (telemetry ? 1u : 0u));
  crc = (packet ^ (packet >> 4) ^ (packet >> 8)) & 0x0fu;
  g_packet[channel] = (uint16_t)((packet << 4) | crc);
}

int am67_dshot_trigger(void)
{
  uint16_t cmp[AM67_DSHOT_NCHANNELS];
  uint32_t idle;
  int bit;
  int ch;

  if (!g_ready || !g_armed)
    {
      return -EAGAIN;
    }

  (void)am67_epwm_tbclk_guard();

  if (g_am67_dshot.busy)
    {
      g_overruns++;
      return -EBUSY;
    }

  /* A compare is never 0: see am67_dshot_fiq.S.  Unused channels send
   * the 0 bit shape behind a force low.
   */

  for (bit = 0; bit < 16; bit++)
    {
      for (ch = 0; ch < AM67_DSHOT_NCHANNELS; ch++)
        {
          cmp[ch] = (g_packet[ch] & (0x8000u >> bit)) ? g_cmp1 : g_cmp0;
        }

      g_am67_dshot_rows[1 + bit][0] = cmp[0] | ((uint32_t)cmp[1] << 16);
      g_am67_dshot_rows[1 + bit][1] = cmp[2] | ((uint32_t)cmp[3] << 16);
      g_am67_dshot_rows[1 + bit][2] = g_csf_bits;
    }

  idle = g_cmp0 | ((uint32_t)g_cmp0 << 16);
  g_am67_dshot_rows[0][0] = idle;
  g_am67_dshot_rows[0][1] = idle;
  g_am67_dshot_rows[0][2] = CSF_IDLE;
  g_am67_dshot_rows[DSHOT_ROWS - 1][0] = idle;
  g_am67_dshot_rows[DSHOT_ROWS - 1][1] = idle;
  g_am67_dshot_rows[DSHOT_ROWS - 1][2] = CSF_IDLE;

  g_am67_dshot.idx = 0;
  g_am67_dshot.busy = 1;
  UP_DSB();

  if (dshot_send(g_timeout_cycles) < 0)
    {
      g_timeouts++;
      return -ETIMEDOUT;
    }

  return OK;
}

void am67_dshot_arm(bool armed)
{
  g_armed = armed;
}

void am67_dshot_stats(struct am67_dshot_stats_s *stats)
{
  stats->frames = g_am67_dshot.frames;
  stats->overruns = g_overruns;
  stats->late = g_am67_dshot.late;
  stats->skipped = g_am67_dshot.skipped;
  stats->timeouts = g_timeouts;
  stats->slow = g_am67_dshot.slow;
  stats->max_exit_ticks = g_am67_dshot.maxcnt;
  stats->max_fiq_cycles = g_am67_dshot.maxcyc;
  stats->frame_cycles = g_am67_dshot.framecyc;
  stats->period_ticks = g_ticks;
}

#endif /* CONFIG_AM67_DSHOT */
