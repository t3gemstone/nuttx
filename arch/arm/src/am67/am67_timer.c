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
#include "am67_clockconfig.h"
#include "am67_irq.h"
 
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
    // Set timer clock source
    clock_unlock();
    *(volatile uint32_t *)(TIMER0_CLOCK_SRC_MUX_ADDR) = TIMER0_CLOCK_SRC_HFOSC0_CLKOUT;
    clock_lock();
    clock_init();
    
    irq_attach(CSLR_R5FSS0_CORE0_INTR_TIMER0_INTR_PEND_0, (xcpt_t)timer_tick_isr, NULL);
    
    //intr_enable();  // We disabled the interrupts during interrupt
                    // initialization, so we enable it here now
    
    up_enable_irq(CSLR_R5FSS0_CORE0_INTR_TIMER0_INTR_PEND_0);

    
    //arm_timer_initialize(25000000);
    
    
    //up_enable_irq(gclock_conf.hw_intr_num, timer_interrupt_handler);
}


void timer_start(uint32_t base_addr)
{
    volatile uint32_t *addr = (uint32_t *)(base_addr + TIMER_TCLR_OFFSET);

    // Start
    *addr |= (0x1U << 0);

}

void timer_stop(uint32_t base_addr)
{
    volatile uint32_t *addr = (volatile uint32_t *)(base_addr + TIMER_TCLR_OFFSET);

    // Stop
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

void clear_overflow_int(uint32_t base_addr)
{
    volatile uint32_t *addr;
    uint32_t value = (0x1U << TIMER_OVF_INT_SHIFT);
    
    addr = (volatile uint32_t *)(base_addr + TIMER_IRQ_STATUS);
    *addr = value;
    
    if ((bool)(*addr & value) == true)	// Make sure interrupt is cleared
        *addr = value;
}

uint32_t is_overflowed(uint32_t base_addr)
{
    uint32_t val;
    
    val = *(volatile uint32_t *)(base_addr + TIMER_IRQ_STATUS_RAW);
    
    return ((val >> TIMER_OVF_INT_SHIFT) & 0x1U);
}

void timer_setup(uint32_t base_addr, struct timer *params)
{    
    volatile uint32_t *addr;
    uint32_t clock_hz, reload_value, ctrl_value, count_value, cycles;

    timer_stop(base_addr);
    clear_overflow_int(base_addr);

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

    if((bool)params->overflow_intr == true)
    {
        addr = (volatile uint32_t *)(base_addr + TIMER_IRQ_INT_ENABLE);
        *addr = (0x1U << TIMER_OVF_INT_SHIFT);
    }
    else
    {
        addr = (volatile uint32_t *)(base_addr + TIMER_IRQ_INT_DISABLE);
        *addr = (0x1U << TIMER_OVF_INT_SHIFT);
    }
}























