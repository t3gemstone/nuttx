/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_timer.c
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
#include <nuttx/arch.h>
#include <nuttx/spinlock.h>
#include <nuttx/timers/arch_alarm.h>
#include <arm_internal.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <syslog.h>
#include <time.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AM67A_MCU_MCU_TIMER0_BASE       (0x04800000u)

/* MCU CTRL MMR Partition 2 for Timer Clock Selection */

#define AM67A_MCU_MCU_CTRL_MMR0_BASE        (0x04500000u)
#define AM67A_MCU_MCU_MMR_LOCK2_KICK0       (0x00009008u)
#define AM67A_MCU_MCU_MMR_LOCK2_KICK1       (0x0000900cu)
#define AM67A_MCU_MCU_MMR_MCU_TIMER0_CLKSEL (0x00008060u)

#define AM67A_MCU_KICK0_UNLOCK_VAL          (0x68ef3490u)
#define AM67A_MCU_KICK1_UNLOCK_VAL          (0xd172bc5au)

/* DMTimer Registers and Bitfields */

#define TIMER_OVF_INT_SHIFT         (0x1)
#define TIMER_IRQ_EOI               (0x20u)
#define TIMER_IRQ_STATUS_RAW        (0x24u)
#define TIMER_IRQ_STATUS            (0x28u)
#define TIMER_IRQ_INT_ENABLE        (0x2cu)
#define TIMER_IRQ_INT_DISABLE       (0x30u)
#define TIMER_TCLR                  (0x38u)
#define TIMER_TCRR                  (0x3cu)
#define TIMER_TLDR                  (0x40u)
#define TIMER_TWPS                  (0x48u)

#define TIMER_TCLR_PEND_MASK        (1u << 0)
#define TIMER_TCRR_PEND_MASK        (1u << 1)
#define TIMER_TLDR_PEND_MASK        (1u << 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct am67a_mcu_timer_params_s
{
  uint32_t input_pre_scaler;
  uint32_t input_clk_hz;
  uint32_t period_in_usec;
  uint32_t period_in_nsec;
  uint32_t oneshot_mode;
  uint32_t enable_overflow_int;
  uint32_t enable_dma_trigger;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void am67a_mcu_timer_wait_twps(uint32_t base_addr, uint32_t mask);
static void am67a_mcu_timer_start(uint32_t base_addr);
static void am67a_mcu_timer_stop(uint32_t base_addr);
static uint32_t am67a_mcu_timer_get_count(uint32_t base_addr);
static void am67a_mcu_timer_clear_overflow_int(uint32_t base_addr);
static void am67a_mcu_timer_params_init(struct am67a_mcu_timer_params_s *params);
static void am67a_mcu_timer_setup(uint32_t base_addr,
                              struct am67a_mcu_timer_params_s *params);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_timer_wait_twps
 *
 * Description:
 *   Wait for previous posted write to complete in DMTimer TWPS register.
 *
 ****************************************************************************/

static inline void am67a_mcu_timer_wait_twps(uint32_t base_addr, uint32_t mask)
{
  while ((getreg32(base_addr + TIMER_TWPS) & mask) != 0)
    {
    }
}

/****************************************************************************
 * Name: am67a_mcu_timer_start
 *
 * Description:
 *   Start timer by setting enable bit in control register.
 *
 ****************************************************************************/

static void am67a_mcu_timer_start(uint32_t base_addr)
{
  uint32_t value;
  uint32_t addr = base_addr + TIMER_TCLR;

  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCLR_PEND_MASK);
  value = getreg32(addr);
  putreg32(value | (0x1u << 0), addr);
  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCLR_PEND_MASK);
}

/****************************************************************************
 * Name: am67a_mcu_timer_stop
 *
 * Description:
 *   Stop timer by clearing enable bit in control register.
 *
 ****************************************************************************/

static void am67a_mcu_timer_stop(uint32_t base_addr)
{
  uint32_t addr = base_addr + TIMER_TCLR;
  uint32_t value;

  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCLR_PEND_MASK);
  value = getreg32(addr);
  putreg32(value & ~(0x1u << 0), addr);
  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCLR_PEND_MASK);
}

/****************************************************************************
 * Name: am67a_mcu_timer_get_count
 *
 * Description:
 *   Read current timer count register.
 *
 ****************************************************************************/

static uint32_t am67a_mcu_timer_get_count(uint32_t base_addr)
{
  return getreg32(base_addr + TIMER_TCRR);
}

/****************************************************************************
 * Name: am67a_mcu_timer_clear_overflow_int
 *
 * Description:
 *   Clear timer overflow interrupt status.
 *
 ****************************************************************************/

static void am67a_mcu_timer_clear_overflow_int(uint32_t base_addr)
{
  uint32_t addr;
  uint32_t value = (0x1u << TIMER_OVF_INT_SHIFT);

  addr = base_addr + TIMER_IRQ_STATUS;
  putreg32(value, addr);

  if ((bool)(getreg32(addr) & value) == true)
    {
      putreg32(value, addr);
    }
}

/****************************************************************************
 * Name: am67a_mcu_timer_params_init
 *
 * Description:
 *   Initialize timer parameters with defaults (25MHz clock, 10ms period).
 *
 ****************************************************************************/

static void am67a_mcu_timer_params_init(struct am67a_mcu_timer_params_s *params)
{
  params->input_pre_scaler = 1;
  params->input_clk_hz = 25 * 1000000;
  params->period_in_usec = 10000;
  params->period_in_nsec = 0;
  params->oneshot_mode = 0;
  params->enable_overflow_int = 1;
  params->enable_dma_trigger = 0;
}

/****************************************************************************
 * Name: am67a_mcu_timer_setup
 *
 * Description:
 *   Configure period, prescaler, reload and interrupts for DMTimer.
 *
 ****************************************************************************/

static void am67a_mcu_timer_setup(uint32_t base_addr,
                              struct am67a_mcu_timer_params_s *params)
{
  uint32_t ctrl_val;
  uint32_t count_val;
  uint32_t reload_val;
  uint64_t time_in_nsec;
  uint64_t input_clk_hz;
  uint64_t timer_cycles;

  am67a_mcu_timer_stop(base_addr);
  am67a_mcu_timer_clear_overflow_int(base_addr);

  time_in_nsec = (uint64_t)params->period_in_nsec;
  if (time_in_nsec == 0U)
    {
      time_in_nsec = params->period_in_usec * 1000U;
    }

  input_clk_hz = params->input_clk_hz / params->input_pre_scaler;
  timer_cycles = (input_clk_hz * time_in_nsec) / 1000000000U;

  count_val = 0xffffffffu - (uint32_t)timer_cycles - 1u;
  reload_val = 0;

  ctrl_val = 0;
  if (params->input_pre_scaler > 1u)
    {
      uint32_t pre_scale_val;

      for (pre_scale_val = 8; pre_scale_val >= 1u; pre_scale_val--)
        {
          if ((params->input_pre_scaler & (0x1u << pre_scale_val)) != 0u)
            {
              break;
            }
        }

      ctrl_val |= (0x1u << 5);
      ctrl_val |= (((pre_scale_val - 1U) & 0x7u) << 2);
    }

  if (params->oneshot_mode == 0u)
    {
      ctrl_val |= (0x1u << 1);
      reload_val = count_val;
    }

  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCLR_PEND_MASK);
  putreg32(ctrl_val, base_addr + TIMER_TCLR);
  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCLR_PEND_MASK);

  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCRR_PEND_MASK);
  putreg32(count_val, base_addr + TIMER_TCRR);
  am67a_mcu_timer_wait_twps(base_addr, TIMER_TCRR_PEND_MASK);

  am67a_mcu_timer_wait_twps(base_addr, TIMER_TLDR_PEND_MASK);
  putreg32(reload_val, base_addr + TIMER_TLDR);
  am67a_mcu_timer_wait_twps(base_addr, TIMER_TLDR_PEND_MASK);

  if ((bool)params->enable_overflow_int == true)
    {
      putreg32((0x1u << TIMER_OVF_INT_SHIFT),
               base_addr + TIMER_IRQ_INT_ENABLE);
    }
  else
    {
      putreg32((0x1u << TIMER_OVF_INT_SHIFT),
               base_addr + TIMER_IRQ_INT_DISABLE);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: timer_tick_isr
 *
 * Description:
 *   Timer tick interrupt service routine.
 *
 ****************************************************************************/

__attribute__((section(".tick_timer")))
int timer_tick_isr(int irq, void *context, void *arg)
{
  am67a_mcu_timer_clear_overflow_int(AM67A_MCU_MCU_TIMER0_BASE);

  nxsched_process_timer();
  return OK;
}

/****************************************************************************
 * Name: up_timer_gettime
 *
 * Description:
 *   Return elapsed time since timer initialization.
 *
 ****************************************************************************/

int up_timer_gettime(struct timespec *ts)
{
  uint64_t internal_timer;

  DEBUGASSERT(ts != NULL);

  internal_timer = am67a_mcu_timer_get_count(AM67A_MCU_MCU_TIMER0_BASE);

  ts->tv_nsec = (uint32_t)(internal_timer * 1000000);
  ts->tv_sec = (uint32_t)(internal_timer / 1000);

  return OK;
}

/****************************************************************************
 * Name: up_timer_start
 *
 * Description:
 *   Start the timer.
 *
 ****************************************************************************/

int up_timer_start(struct timespec const *ts)
{
  DEBUGASSERT(ts != NULL);
  am67a_mcu_timer_start(AM67A_MCU_MCU_TIMER0_BASE);
  return OK;
}

/****************************************************************************
 * Name: up_timer_cancel
 *
 * Description:
 *   Stop the timer.
 *
 ****************************************************************************/

int up_timer_cancel(struct timespec *ts)
{
  DEBUGASSERT(ts != NULL);
  am67a_mcu_timer_stop(AM67A_MCU_MCU_TIMER0_BASE);
  return OK;
}

/****************************************************************************
 * Name: up_timer_initialize
 *
 * Description:
 *   Initialize DMTimer0 hardware for system tick generation.
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  struct am67a_mcu_timer_params_s params;

  /* 1. Unlock MCU CTRL MMR Partition 2 */

  putreg32(AM67A_MCU_KICK0_UNLOCK_VAL,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK0);
  putreg32(AM67A_MCU_KICK1_UNLOCK_VAL,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK1);

  /* 2. Select HFOSC0 (25MHz) clock for MCU_TIMER0 */

  putreg32(0x0u,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_MCU_TIMER0_CLKSEL);

  /* 3. Re-lock MCU CTRL MMR Partition 2 */

  putreg32(0x0u,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK0);
  putreg32(0x0u,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK1);

  syslog(LOG_INFO, "[TIMER] MCU_TIMER0 clock configured (25MHz)\n");

  am67a_mcu_timer_params_init(&params);

  up_disable_irq(AM67A_MCU_IRQ_MCU_TIMER0);
  irq_attach(AM67A_MCU_IRQ_MCU_TIMER0, timer_tick_isr, NULL);

  am67a_mcu_timer_stop(AM67A_MCU_MCU_TIMER0_BASE);
  am67a_mcu_timer_setup(AM67A_MCU_MCU_TIMER0_BASE, &params);
  am67a_mcu_timer_start(AM67A_MCU_MCU_TIMER0_BASE);

  up_enable_irq(AM67A_MCU_IRQ_MCU_TIMER0);
  syslog(LOG_INFO, "[TIMER] MCU_TIMER0 started (IRQ %d)\n",
         AM67A_MCU_IRQ_MCU_TIMER0);
}
