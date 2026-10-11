/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_hrt.c
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
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/queue.h>

#include "arm_internal.h"
#include <arch/chip/irq.h>
#include <arch/chip/am67a_mcu_hrt.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Hardware Base Addresses */

#define AM67A_MCU_MCU_TIMER1_BASE           (0x04810000u)
#define AM67A_MCU_MCU_CTRL_MMR0_BASE        (0x04500000u)

/* MCU CTRL MMR Partition 2 Offsets & Unlock Keys */

#define AM67A_MCU_MCU_MMR_LOCK2_KICK0       (0x00009008u)
#define AM67A_MCU_MCU_MMR_LOCK2_KICK1       (0x0000900cu)
#define AM67A_MCU_MCU_MMR_MCU_TIMER1_CLKSEL (0x00008064u)

#define AM67A_MCU_KICK0_UNLOCK_VAL          (0x68ef3490u)
#define AM67A_MCU_KICK1_UNLOCK_VAL          (0xd172bc5au)

/* DMTimer Registers */

#define TIMER_IRQ_EOI                       (0x0020u)
#define TIMER_IRQ_STATUS_RAW                (0x0024u)
#define TIMER_IRQ_STATUS                    (0x0028u)
#define TIMER_IRQ_INT_ENABLE                (0x002cu)
#define TIMER_IRQ_INT_DISABLE               (0x0030u)
#define TIMER_TCLR                          (0x0038u)
#define TIMER_TCRR                          (0x003cu)
#define TIMER_TLDR                          (0x0040u)
#define TIMER_TWPS                          (0x0048u)
#define TIMER_TMAR                          (0x004cu)

/* DMTimer TCLR Control Bits */

#define TIMER_TCLR_ST                       (1u << 0)   /* Start timer */
#define TIMER_TCLR_AR                       (1u << 1)   /* Auto-reload */
#define TIMER_TCLR_CE                       (1u << 6)   /* Compare enable (match) */

/* DMTimer Interrupt Flags */

#define TIMER_IRQ_MAT                       (1u << 0)   /* Match interrupt */
#define TIMER_IRQ_OVF                       (1u << 1)   /* Overflow interrupt */

/* DMTimer TWPS Write Pending Masks */

#define TIMER_TWPS_TCLR                     (1u << 0)
#define TIMER_TWPS_TCRR                     (1u << 1)
#define TIMER_TWPS_TLDR                     (1u << 2)
#define TIMER_TWPS_TMAR                     (1u << 4)

/* Timer Clock Frequency (HFOSC0 = 25 MHz) */

#define HRT_HW_FREQ_HZ                      (25000000ULL)
#define HRT_CYCLES_PER_USEC                 (25ULL)

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* 64-bit cycle accumulator for rollover handling */

static volatile uint64_t g_hrt_base_cycles;
static volatile uint32_t g_hrt_last_count;

/* Priority queue of scheduled callouts (sorted by deadline) */

static sq_queue_t g_callout_queue;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_hrt_wait_twps
 ****************************************************************************/

static inline void am67a_mcu_hrt_wait_twps(uint32_t mask)
{
  while ((getreg32(AM67A_MCU_MCU_TIMER1_BASE + TIMER_TWPS) & mask) != 0)
    {
    }
}

/****************************************************************************
 * Name: hrt_reschedule
 *
 * Description:
 *   Re-programs the hardware match register (TMAR) for the next callout
 *   at the head of the priority queue. Must be called in critical section.
 ****************************************************************************/

static void hrt_reschedule(void)
{
  struct hrt_call *next;
  hrt_abstime now;
  uint64_t delay_us;
  uint64_t delay_cycles;
  uint32_t current_count;
  uint32_t target_tmar;

  next = (struct hrt_call *)sq_peek(&g_callout_queue);
  if (next == NULL)
    {
      /* No pending callouts: disable match interrupt */

      putreg32(TIMER_IRQ_MAT,
               AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_INT_DISABLE);
      return;
    }

  now = hrt_absolute_time();
  if (next->deadline <= now)
    {
      delay_us = 1;
    }
  else
    {
      delay_us = next->deadline - now;
    }

  delay_cycles = delay_us * HRT_CYCLES_PER_USEC;
  if (delay_cycles > 0x7fffffffull)
    {
      delay_cycles = 0x7fffffffull;
    }

  current_count = getreg32(AM67A_MCU_MCU_TIMER1_BASE + TIMER_TCRR);
  target_tmar   = current_count + (uint32_t)delay_cycles;

  am67a_mcu_hrt_wait_twps(TIMER_TWPS_TMAR);
  putreg32(target_tmar, AM67A_MCU_MCU_TIMER1_BASE + TIMER_TMAR);

  /* Clear any stale match flag and enable match interrupt */

  putreg32(TIMER_IRQ_MAT, AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_STATUS);
  putreg32(TIMER_IRQ_MAT, AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_INT_ENABLE);
}

/****************************************************************************
 * Name: hrt_enqueue_call
 *
 * Description:
 *   Inserts a callout into the priority queue sorted in ascending order
 *   of deadline. Must be called in critical section.
 ****************************************************************************/

static void hrt_enqueue_call(struct hrt_call *entry)
{
  struct hrt_call *curr;
  struct hrt_call *prev;

  prev = NULL;
  curr = (struct hrt_call *)sq_peek(&g_callout_queue);

  while (curr != NULL && curr->deadline <= entry->deadline)
    {
      prev = curr;
      curr = (struct hrt_call *)sq_next(&curr->link);
    }

  if (prev == NULL)
    {
      sq_addfirst(&entry->link, &g_callout_queue);
    }
  else
    {
      sq_addafter(&prev->link, &entry->link, &g_callout_queue);
    }
}

/****************************************************************************
 * Name: hrt_tim_isr
 *
 * Description:
 *   Interrupt handler for MCU_TIMER1 (IRQ 29).
 *   Handles both 32-bit rollover and match/compare events.
 ****************************************************************************/

static int hrt_tim_isr(int irq, void *context, void *arg)
{
  uint32_t status;
  hrt_abstime now;
  struct hrt_call *call;
  hrt_abstime period;
  hrt_callout cb;
  void *cb_arg;

  status = getreg32(AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_STATUS);

  /* 1. Handle Counter Overflow (32-bit wrap) */

  if ((status & TIMER_IRQ_OVF) != 0)
    {
      uint32_t count = getreg32(AM67A_MCU_MCU_TIMER1_BASE + TIMER_TCRR);

      if (count < g_hrt_last_count)
        {
          g_hrt_base_cycles += 0x100000000ull;
          g_hrt_last_count   = count;
        }

      putreg32(TIMER_IRQ_OVF, AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_STATUS);
    }

  /* 2. Handle Match Event (Callout Deadline Expiration) */

  if ((status & TIMER_IRQ_MAT) != 0)
    {
      putreg32(TIMER_IRQ_MAT, AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_STATUS);

      now = hrt_absolute_time();

      while (true)
        {
          call = (struct hrt_call *)sq_peek(&g_callout_queue);
          if (call == NULL || call->deadline > now)
            {
              break;
            }

          sq_rem(&call->link, &g_callout_queue);

          period   = call->period;
          cb       = call->callout;
          cb_arg   = call->arg;

          /* Mark callout as completed (0 deadline) */

          call->deadline = 0;

          if (cb != NULL)
            {
              cb(cb_arg);
            }

          /* If periodic, re-queue with next deadline */

          if (period != 0)
            {
              call->deadline = now + period;
              hrt_enqueue_call(call);
            }
        }

      hrt_reschedule();
    }

  /* Write EOI acknowledgement */

  putreg32(0, AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_EOI);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hrt_absolute_time
 *
 * Description:
 *   Return current monotonic time in microseconds since system boot.
 ****************************************************************************/

hrt_abstime hrt_absolute_time(void)
{
  irqstate_t flags;
  uint32_t count;
  uint64_t total_cycles;

  flags = enter_critical_section();

  count = getreg32(AM67A_MCU_MCU_TIMER1_BASE + TIMER_TCRR);

  if (count < g_hrt_last_count)
    {
      g_hrt_base_cycles += 0x100000000ull;
    }

  g_hrt_last_count = count;
  total_cycles     = g_hrt_base_cycles + count;

  leave_critical_section(flags);

  return (total_cycles / HRT_CYCLES_PER_USEC);
}

/****************************************************************************
 * Name: hrt_store_absolute_time
 ****************************************************************************/

void hrt_store_absolute_time(volatile hrt_abstime *t)
{
  irqstate_t flags = enter_critical_section();
  *t = hrt_absolute_time();
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: hrt_call_after
 ****************************************************************************/

void hrt_call_after(struct hrt_call *entry, hrt_abstime delay,
                    hrt_callout callout, void *arg)
{
  hrt_call_every(entry, delay, 0, callout, arg);
}

/****************************************************************************
 * Name: hrt_call_at
 ****************************************************************************/

void hrt_call_at(struct hrt_call *entry, hrt_abstime calltime,
                 hrt_callout callout, void *arg)
{
  irqstate_t flags;
  hrt_abstime now;

  DEBUGASSERT(entry != NULL);

  flags = enter_critical_section();

  now = hrt_absolute_time();
  hrt_cancel(entry);

  entry->period   = 0;
  entry->callout  = callout;
  entry->arg      = arg;
  entry->deadline = calltime > now ? calltime : now;

  hrt_enqueue_call(entry);
  hrt_reschedule();

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: hrt_call_every
 ****************************************************************************/

void hrt_call_every(struct hrt_call *entry, hrt_abstime delay,
                    hrt_abstime interval, hrt_callout callout, void *arg)
{
  irqstate_t flags;
  hrt_abstime now;

  DEBUGASSERT(entry != NULL);

  flags = enter_critical_section();

  now = hrt_absolute_time();
  hrt_cancel(entry);

  entry->period   = interval;
  entry->callout  = callout;
  entry->arg      = arg;
  entry->deadline = now + delay;

  hrt_enqueue_call(entry);
  hrt_reschedule();

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: hrt_cancel
 ****************************************************************************/

void hrt_cancel(struct hrt_call *entry)
{
  irqstate_t flags;
  struct hrt_call *head_before;
  struct hrt_call *head_after;

  DEBUGASSERT(entry != NULL);

  flags = enter_critical_section();

  head_before = (struct hrt_call *)sq_peek(&g_callout_queue);
  sq_rem(&entry->link, &g_callout_queue);
  entry->deadline = 0;
  head_after  = (struct hrt_call *)sq_peek(&g_callout_queue);

  if (head_before != head_after)
    {
      hrt_reschedule();
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: hrt_called
 ****************************************************************************/

bool hrt_called(struct hrt_call *entry)
{
  DEBUGASSERT(entry != NULL);
  return (entry->deadline == 0);
}

/****************************************************************************
 * Name: hrt_init
 *
 * Description:
 *   Initialize MCU_TIMER1 for 25 MHz free-running operation, enable
 *   overflow and match interrupts, and attach IRQ 29.
 ****************************************************************************/

void hrt_init(void)
{
  irqstate_t flags;

  flags = enter_critical_section();

  /* 1. Unlock MCU CTRL MMR Partition 2 */

  putreg32(AM67A_MCU_KICK0_UNLOCK_VAL,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK0);
  putreg32(AM67A_MCU_KICK1_UNLOCK_VAL,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK1);

  /* 2. Select HFOSC0 (25MHz) clock for MCU_TIMER1 */

  putreg32(0x0u, AM67A_MCU_MCU_CTRL_MMR0_BASE +
                 AM67A_MCU_MCU_MMR_MCU_TIMER1_CLKSEL);

  /* 3. Re-lock MCU CTRL MMR Partition 2 */

  putreg32(0x0u,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK0);
  putreg32(0x0u,
           AM67A_MCU_MCU_CTRL_MMR0_BASE + AM67A_MCU_MCU_MMR_LOCK2_KICK1);

  /* 4. Stop timer and clear pending status */

  am67a_mcu_hrt_wait_twps(TIMER_TWPS_TCLR);
  putreg32(0, AM67A_MCU_MCU_TIMER1_BASE + TIMER_TCLR);

  putreg32(TIMER_IRQ_OVF | TIMER_IRQ_MAT,
           AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_STATUS);

  /* 5. Set counter and reload register to 0 (free-running up counter) */

  am67a_mcu_hrt_wait_twps(TIMER_TWPS_TCRR);
  putreg32(0, AM67A_MCU_MCU_TIMER1_BASE + TIMER_TCRR);

  am67a_mcu_hrt_wait_twps(TIMER_TWPS_TLDR);
  putreg32(0, AM67A_MCU_MCU_TIMER1_BASE + TIMER_TLDR);

  /* 6. Enable overflow interrupt initially (match is enabled on callout) */

  putreg32(TIMER_IRQ_OVF, AM67A_MCU_MCU_TIMER1_BASE + TIMER_IRQ_INT_ENABLE);

  /* 7. Start timer with Auto-Reload and Compare Enable */

  am67a_mcu_hrt_wait_twps(TIMER_TWPS_TCLR);
  putreg32(TIMER_TCLR_ST | TIMER_TCLR_AR | TIMER_TCLR_CE,
           AM67A_MCU_MCU_TIMER1_BASE + TIMER_TCLR);

  /* 8. Initialize state variables & callout queue */

  g_hrt_base_cycles = 0;
  g_hrt_last_count  = 0;
  sq_init(&g_callout_queue);

  /* 9. Attach and enable IRQ 29 */

  up_disable_irq(AM67A_MCU_IRQ_MCU_TIMER1);
  irq_attach(AM67A_MCU_IRQ_MCU_TIMER1, hrt_tim_isr, NULL);
  up_enable_irq(AM67A_MCU_IRQ_MCU_TIMER1);

  leave_critical_section(flags);
  syslog(LOG_INFO, "[HRT] MCU_TIMER1 initialized at 25 MHz (IRQ %d)\n",
         AM67A_MCU_IRQ_MCU_TIMER1);
}
