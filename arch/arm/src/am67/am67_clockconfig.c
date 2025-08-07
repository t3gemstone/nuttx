/****************************************************************************
 * arch/arm/src/am67/am67_clockconfig.c
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
 
 /*
 *  Copyright (C) 2021 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/
 
#include <nuttx/config.h>
#include <stdio.h>

#include "arm_internal.h"
#include "am67_clockconfig.h"
#include "hardware/am67_memorymap.h"

#include "am67_timer.h"
#include "am67_irq.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

struct intr_ctrl gintr_ctrl;


struct clock_ctrl gclock_ctrl;
struct clock_conf gclock_conf =
{
    .period_usec = 1000,
    .base_addr = TIMER0_BASE_ADDR,
    .hw_intr_num = 24,
    .clock_hz = 25000000,
    .prescaler = 1
};

void clock_unlock(void)
{
    uint32_t base_addr;
    volatile uint32_t *kick_addr;

    base_addr = CSL_CTRL_MMR0_CFG0_BASE;
    kick_addr = (volatile uint32_t *)(base_addr + CSL_MAIN_CTRL_MMR_LOCKn_KICK0_OFFSET(2));
    CSL_REG32_WR(kick_addr, KICK0_UNLOCK_VAL);
    kick_addr++;
    CSL_REG32_WR(kick_addr, KICK1_UNLOCK_VAL);
}

void clock_lock(void)
{
    uint32_t base_addr;
    volatile uint32_t *kick_addr;
    
    base_addr = CSL_CTRL_MMR0_CFG0_BASE;
    kick_addr = (volatile uint32_t *)(base_addr + CSL_MAIN_CTRL_MMR_LOCKn_KICK0_OFFSET(2));
    CSL_REG32_WR(kick_addr, KICK_LOCK_VAL);
    kick_addr++;
    CSL_REG32_WR(kick_addr, KICK_LOCK_VAL);
}

int timer_tick_isr(int irq, void *context, void *arg)
{
    gclock_ctrl.ticks++;

    if ((gclock_ctrl.clock.timeout != 0) && (gclock_ctrl.clock.timeout  == gclock_ctrl.ticks))
    {
        if (gclock_ctrl.clock.period == 0)
            gclock_ctrl.clock.timeout = 0;
        else
            gclock_ctrl.clock.timeout = gclock_ctrl.ticks + gclock_ctrl.clock.period;

        if (gclock_ctrl.clock.callback != NULL)
            gclock_ctrl.clock.callback(irq, NULL, gclock_ctrl.clock.args);
    }
    clear_overflow_int(gclock_ctrl.base_addr);
    
    return OK;
}

void clock_init(void)
{
    struct timer timer_params;
    struct intr intr_params;
    
    gclock_ctrl.ticks = 0;
    gclock_ctrl.period_usec = gclock_conf.period_usec;
    gclock_ctrl.base_addr = gclock_conf.base_addr;

    // Default parameters
    timer_params.clock_hz = 25 * 1000000;
    timer_params.prescaler = 1;
    timer_params.period_usec = 1000;
    timer_params.one_shot = 0;
    timer_params.overflow_intr = 1;		// Enable interrupts
    timer_params.dma = 0;
    
    // Configuration parameters
    timer_params.prescaler = gclock_conf.prescaler;
    timer_params.clock_hz = gclock_conf.clock_hz;
    timer_params.period_usec = gclock_conf.period_usec;
    
    timer_setup(gclock_ctrl.base_addr, &timer_params);

    gclock_ctrl.reload_count = get_reload(gclock_ctrl.base_addr);
    
    // Default parameters

    intr_params.intr_num = 0;
    intr_params.callback = NULL;
    intr_params.args = NULL;
    intr_params.event_id = 0;
    intr_params.priority = (INTR_MAX_PRIORITY - 1U);
    // intr_params.is_fiq = 0;
    // intr_params.is_pulse = 0;

    intr_params.intr_num = gclock_conf.hw_intr_num;
    intr_params.callback = timer_tick_isr;

    disable_intr(intr_params.intr_num);
    clear_intr(intr_params.intr_num);
    intr_set_priority(intr_params.intr_num, intr_params.priority);

    set_vector((uint32_t)intr_params.intr_num, (uintptr_t)irq_handler);

    gintr_ctrl.isr[intr_params.intr_num] = intr_params.callback;
    gintr_ctrl.isr_args[intr_params.intr_num] = intr_params.args;

    enable_intr(intr_params.intr_num);
    
    irq_attach(intr_params.intr_num, timer_tick_isr, NULL);
    up_enable_irq(intr_params.intr_num); // NuttX function

    timer_start(gclock_ctrl.base_addr);
}

void clock_deinit(void)
{
    timer_stop(gclock_ctrl.base_addr);

    // Do some interrupt related things here
}
