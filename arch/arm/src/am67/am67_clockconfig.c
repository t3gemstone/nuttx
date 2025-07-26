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

/****************************************************************************
 * Included Files
 ****************************************************************************/
 
#include <nuttx/config.h>
#include <stdio.h>

#include "arm_internal.h"
#include "am67_clockconfig.h"
#include "hardware/am67_memorymap.h"

#include "am67_timer.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

struct clock_ctrl gclock_ctrl;
struct clock_conf gclock_conf;


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

void clock_init(void)
{
    // Set the timer clock source


    *(volatile uint32_t *)(TIMER0_CLOCK_SRC_MUX_ADDR) = TIMER0_CLOCK_SRC_HFOSC0_CLKOUT;

    struct timer timer_params;
    uint32_t reload;

    // This is defined somewhere else in the original,
    // defined it here for simplicity.
    gclock_conf.period_usec = 1000;
    gclock_conf.base_addr = TIMER0_BASE_ADDR;

    gclock_ctrl.ticks = 0;
    gclock_ctrl.period_usec = gclock_conf.period_usec;
    gclock_ctrl.base_addr = gclock_conf.base_addr;

    timer_params.clock_hz = 25 * 1000000;
    timer_params.prescaler = 1;
    timer_params.period_usec = 1000;
    timer_params.one_shot = 0;
    timer_params.interrupt = 0;
    timer_params.dma = 0;
    
    timer_setup(TIMER0_BASE_ADDR, &timer_params);

    reload = get_reload(gclock_conf.base_addr);
    
    // Do interrupt related things here

    timer_start(TIMER0_BASE_ADDR);
}

void clock_deinit(void)
{
    timer_stop(gclock_ctrl.base_addr);

    // Do some interrup related things here
}
