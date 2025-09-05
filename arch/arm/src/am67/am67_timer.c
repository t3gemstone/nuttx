/* Copyright (C) 2021 Texas Instruments Incorporated */
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

#include <fcntl.h>
#include <nuttx/config.h>
#include <nuttx/timers/arch_alarm.h>

#include "arm_internal.h"
#include <nuttx/arch.h>
#include <nuttx/spinlock.h>
#include <stdint.h>
#include <time.h>
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


/* GP timer implementation for clock tick */

#define TIMER_IRQ_EOI           (0x20u)
#define TIMER_IRQ_STATUS_RAW    (0x24u)
#define TIMER_IRQ_STATUS        (0x28u)
#define TIMER_IRQ_INT_ENABLE    (0x2Cu)
#define TIMER_IRQ_INT_DISABLE   (0x30u)
#define TIMER_TCLR              (0x38u)
#define TIMER_TCRR              (0x3cu)
#define TIMER_TLDR              (0x40u)

#define TIMER_OVF_INT_SHIFT     (0x1)

int uart_Test(int irq, void *context, void *arg);
void up_timer_initialize(void)
{
    Timer_Params params;
    TimerP_Params_init(&params);
    up_disable_irq(CSLR_R5FSS0_CORE0_INTR_TIMER0_INTR_PEND_0);
    irq_attach(CSLR_R5FSS0_CORE0_INTR_TIMER0_INTR_PEND_0, timer_tick_isr, NULL);
    TimerP_stop(AM67_DMTIMER1_1MS_TIMER0_VADDR);
    TimerP_setup(AM67_DMTIMER1_1MS_TIMER0_VADDR,&params);
    TimerP_start(AM67_DMTIMER1_1MS_TIMER0_VADDR);    
    up_enable_irq(CSLR_R5FSS0_CORE0_INTR_TIMER0_INTR_PEND_0);
}




__attribute__((section(".tickTimer")))
int timer_tick_isr(int irq, void *context, void *arg)
{ 
    TimerP_clearOverflowInt(AM67_DMTIMER1_1MS_TIMER0_VADDR);
   
    nxsched_process_timer();
    return OK;
}



int up_timer_gettime(struct timespec* ts)
{
	return TimerP_getCount(AM67_DMTIMER1_1MS_TIMER0_VADDR);
}

int up_timer_start(struct timespec const* ts)
{
	TimerP_start(AM67_DMTIMER1_1MS_TIMER0_VADDR);
	return 0;
}

int up_timer_cancel(struct timespec* ts)
{
	TimerP_stop(AM67_DMTIMER1_1MS_TIMER0_VADDR);
	return 0;
}


void TimerP_Params_init(Timer_Params *params)
{
    params->inputPreScaler = 1;
    params->inputClkHz = 25*1000000;
    params->periodInUsec = 1000;
    params->periodInNsec = 0; /* periodInUsec is used when periodInNsec is 0 */
    params->oneshotMode = 0;
    params->enableOverflowInt = 1;
    params->enableDmaTrigger = 0; /* NOT USED */
}

void TimerP_setup(uint32_t baseAddr, Timer_Params *params)
{
    volatile uint32_t *addr;
    uint32_t ctrlVal;
    uint32_t countVal, reloadVal;
    uint64_t timeInNsec, inputClkHz, timerCycles;

    

    /* stop timer and clear pending interrupts */
    TimerP_stop(baseAddr);
    TimerP_clearOverflowInt(baseAddr);

    timeInNsec = (uint64_t)params->periodInNsec;
    if(timeInNsec == 0U)
    {
        timeInNsec = params->periodInUsec*1000U;
    }

    inputClkHz = params->inputClkHz / params->inputPreScaler;
    timerCycles =  ( inputClkHz * timeInNsec ) / 1000000000U;

    /* if timerCycles > 32b then we cannot give accurate timing */

    /* calculate count and reload value register value */
    countVal = 0xFFFFFFFFu - (uint32_t)timerCycles - 1U;

    /* keep reload value as 0, later if is auto-reload is enabled, it will be set a value > 0 */
    reloadVal = 0;

    /* calculate control register value, keep timer disabled */
    ctrlVal = 0;
    if(params->inputPreScaler>1U)
    {
        uint32_t preScaleVal;

        for(preScaleVal=8; preScaleVal>=1U; preScaleVal--)
        {
            if( (params->inputPreScaler & (0x1U << preScaleVal)) != 0U )
            {
                break;
            }
        }

        /* enable pre-scaler */
        ctrlVal |= (0x1U << 5);
        /* set pre-scaler value */
        ctrlVal |= ( ((preScaleVal - 1U) & 0x7U) << 2);
    }
    if(params->oneshotMode==0U)
    {
        /* autoreload timer */
        ctrlVal |= (0x1U << 1);
        reloadVal = countVal;
    }

    /* set timer control value */
    addr = (volatile uint32_t *)(baseAddr + TIMER_TCLR);
    *addr = ctrlVal;

    /* set timer count value */
    addr = (volatile uint32_t *)(baseAddr + TIMER_TCRR);
    *addr = countVal;

    /* set reload value */
    addr = (volatile uint32_t *)(baseAddr + TIMER_TLDR);
    *addr = reloadVal;

    /* enable/disable interrupts */
    if((bool)params->enableOverflowInt == true)
    {
        /* enable interrupt */
        addr = (volatile uint32_t *)(baseAddr + TIMER_IRQ_INT_ENABLE);
        *addr = (0x1U << TIMER_OVF_INT_SHIFT);
    }
    else
    {
        /* disable interrupt */
        addr = (volatile uint32_t *)(baseAddr + TIMER_IRQ_INT_DISABLE);
        *addr = (0x1U << TIMER_OVF_INT_SHIFT);
    }
}

void TimerP_start(uint32_t baseAddr)
{
    volatile uint32_t *addr = (uint32_t *)(baseAddr + TIMER_TCLR);

    /* start timer */
    *addr |= (0x1U << 0);
}

void TimerP_stop(uint32_t baseAddr)
{
    volatile uint32_t *addr = (volatile uint32_t *)(baseAddr + TIMER_TCLR);

    /* stop timer */
    *addr &= ~(0x1U << 0);
}

uint32_t TimerP_getCount(uint32_t baseAddr)
{
    volatile uint32_t *addr = (volatile uint32_t *)(baseAddr + TIMER_TCRR);

    return *addr;
}

uint32_t TimerP_getReloadCount(uint32_t baseAddr)
{
    volatile uint32_t *addr = (volatile uint32_t *)(baseAddr + TIMER_TLDR);

    return *addr;
}

void TimerP_clearOverflowInt(uint32_t baseAddr)
{
    volatile uint32_t *addr;
    uint32_t value = (0x1U << TIMER_OVF_INT_SHIFT);

    /* clear status for overflow interrupt */
    addr = (volatile uint32_t *)(baseAddr + TIMER_IRQ_STATUS);
    *addr = value;

    /* [MCUSDK-177] read back and make sure interrupt was indeed cleared, if not clear it again
     */
    if((bool)(*addr & value) == true)
    {
        *addr = value;
    }

    #if 0 /* should not be used for level interrupts */
    /* apply SW EOI */
    addr = (volatile uint32_t *)(baseAddr + TIMER_IRQ_EOI);
    *addr = 0;
    #endif

}

uint32_t TimerP_isOverflowed(uint32_t baseAddr)
{
    uint32_t val;

    /* get status for overflow interrupt */
    val = *(volatile uint32_t *)(baseAddr + TIMER_IRQ_STATUS_RAW);

    return ((val >> TIMER_OVF_INT_SHIFT) & 0x1U);
}
