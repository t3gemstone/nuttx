/****************************************************************************
 * arch/arm/src/armv7-r/arm_doirq.c
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

#include <stdint.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/board.h>
#include <arch/board/board.h>
#include <sched/sched.h>

#include "arm_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

uint32_t *arm_doirq(int irq, uint32_t *regs)
{
  struct tcb_s *tcb = this_task();

  board_autoled_on(LED_INIRQ);

#ifdef CONFIG_SUPPRESS_INTERRUPTS
  PANIC();
#else
  /* Nested interrupts are not supported */

  DEBUGASSERT(!up_interrupt_context());

  /* if irq == GIC_SMP_CPUSTART
   * We are initiating the multi-core jumping state to up_idle,
   * and we will use this_task(). Therefore, it cannot be overridden.
   */

#ifdef CONFIG_SMP
  if (irq != GIC_SMP_CPUSTART)
#endif
    {
      /* The frame belongs to the interrupted task. If the ready list head
       * already differs from it, the frame is stored in the wrong TCB and
       * the interrupted task keeps a stale or NULL context (report the
       * first few; see the NULL check below).
       */

      static int s_mismatch_reports;

      if (tcb != g_running_tasks[this_cpu()] && s_mismatch_reports < 4)
        {
          s_mismatch_reports++;
          _alert("irq %d entry: this_task %s (pid %d) != running %s\n",
                 irq, tcb->name, tcb->pid,
                 g_running_tasks[this_cpu()] != NULL ?
                 g_running_tasks[this_cpu()]->name : "-");
        }

      tcb->xcp.regs = regs;
    }

  /* Set irq flag */

  up_set_interrupt_context(true);

  /* Deliver the IRQ */

  irq_dispatch(irq, regs);
  tcb = this_task();

  if (regs != tcb->xcp.regs)
    {
      struct tcb_s **running_task = &g_running_tasks[this_cpu()];

      /* Update scheduler parameters */

      nxsched_switch_context(*running_task, tcb);

      /* Record the new "running" task when context switch occurred.
       * g_running_tasks[] is only used by assertion logic for reporting
       * crashes.
       */

      *running_task = tcb;
      regs = tcb->xcp.regs;

      /* Returning NULL makes the exception return load the next context
       * from address 0 (seen once in a 6 h soak on the AM67: undefined
       * instruction in the idle task with code words in every register).
       * Stop with a readable report instead.
       */

      if (regs == NULL)
        {
          _alert("NULL context: irq %d switching to %s (pid %d)\n",
                 irq, tcb->name, tcb->pid);
          PANIC();
        }
    }

  /* Set irq flag */

  up_set_interrupt_context(false);
  board_autoled_off(LED_INIRQ);
#endif
  return regs;
}
