/****************************************************************************
 * arch/arm/src/am67/am67_timerisr.c
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
 #include <nuttx/timers/arch_alarm.h>
 
 #include "arm_internal.h"
 #include "am67_timer.h"
 
 /****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function:  up_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize
 *   the timer interrupt.
 *
 ****************************************************************************/


void up_timer_initialize(void)
{
}

void timer_start(uint32_t base_addr)
{
    volatile uint32_t *addr = (uint32_t *)(base_addr + TIMER_TCLR_OFFSET);

    // Stop
    *addr |= (0x1U << 0);

}

void timer_stop(uint32_t base_addr)
{
    volatile uint32_t *addr = (volatile uint32_t *)(base_addr + TIMER_TCLR_OFFSET);

    // Start
    *addr &= ~(0x1U << 0);
}

uint32_t get_count(uint32_t base_addr)
{
    volatile uint32_t *addr = (volatile uint32_t *)(base_addr + TIMER_TCRR_OFFSET);
    return *addr;
}

uint32_t get_reload(uint32_t base_addr)
{
    volatile uint32_t *addr = (volatile uint32_t *)(base_addr + TIMER_TLDR_OFFSET);
    return *addr;
}

void timer_setup(uint32_t base_addr, struct timer *params)
{
    volatile uint32_t *addr;
    uint32_t clock_hz, reload_value, ctrl_value, count_value, cycles;

    timer_stop(base_addr);

    clock_hz = params-> clock_hz / params->prescaler;
    cycles = clock_hz / 1000000U;

    count_value = 0xFFFFFFFFu - (uint32_t)cycles - 1U;

    reload_value = 0;
    ctrl_value = 0;

    if (params->one_shot == 0U)     // Autoreload
    {
        ctrl_value |= (0x1U << 1);
        reload_value = count_value;
    }

    // Set the timer control
    addr = (volatile uint32_t *)(base_addr + TIMER_TCLR_OFFSET);
    *addr = ctrl_value;

    // Set the timer count
    addr = (volatile uint32_t *)(base_addr + TIMER_TCRR_OFFSET);
    *addr = count_value;

    // Set the reload
    addr = (volatile uint32_t *)(base_addr + TIMER_TLDR_OFFSET);
    *addr = reload_value;

    // Some interrupt related settings can be done here
    // Refer to enableOverflowInt()

}























