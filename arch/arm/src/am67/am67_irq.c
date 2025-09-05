/* Copyright (C) 2021 Texas Instruments Incorporated */
/****************************************************************************
 * arch/arm/src/am67/am67_irq.c
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

#include <assert.h>
#include <nuttx/arch.h>
#include <nuttx/config.h>
#include <nuttx/irq.h>

#include "arm_internal.h"
// #include "irq/irq.h"
#include "am67_gpio.h"
#include "am67_irq.h"
#include "am67_timer.h"
#include "irq/irq.h"
#include "sctlr.h"

/*TODO: fill that function according to TI implementation */


static volatile uint32_t gdummy;

HwiP_Config gHwiConfig;

extern int irq_unexpected_isr(int irq, FAR void *context, FAR void *arg);
extern uint32_t *arm_doirq(int irq, uint32_t *regs);

void HwiP_enableInt(uint32_t intNum)
{
    volatile uint32_t *addr;
    uint32_t bitPos;

    Utils_dataAndInstructionBarrier();

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_EN(intNum));
    bitPos = VIM_BIT_POS(intNum);

    *addr = (0x1u << bitPos);
}

uint32_t HwiP_disableInt(uint32_t intNum)
{
    volatile uint32_t *addr;
    uint32_t bitPos;
    uint32_t isEnable = 0;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_DIS(intNum));
    bitPos = VIM_BIT_POS(intNum);

    if ((*addr & ((uint32_t)0x1 << bitPos)) != 0U)
    {
        isEnable = 1;
    }
    *addr = ((uint32_t)0x1 << bitPos);

    Utils_dataAndInstructionBarrier();

    return isEnable;
}

void HwiP_restoreInt(uint32_t intNum, uint32_t oldIntState)
{
    if (oldIntState != 0U)
    {
        HwiP_enableInt(intNum);
    }
    else
    {
        (void)HwiP_disableInt(intNum);
    }
}

void HwiP_clearInt(uint32_t intNum)
{
    volatile uint32_t *addr;
    uint32_t bitPos;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_STS(intNum));
    bitPos = VIM_BIT_POS(intNum);

    *addr = (0x1u << bitPos);
}

void HwiP_post(uint32_t intNum)
{
    volatile uint32_t *addr;
    uint32_t bitPos;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_RAW(intNum));
    bitPos = VIM_BIT_POS(intNum);

    *addr = (0x1u << bitPos);

    /*
     * Add delay to insure posted interrupt are triggered before function
     * returns.
     */

    Utils_dataAndInstructionBarrier();
}

void up_irqinitialize(void)
{
    sched_lock();
    AM67_irq_init();
    sched_unlock();
}

void AM67_irq_init(void)
{
    gHwiConfig.intcBaseAddr = 0x2FFF0000u;

    (void)AM67_disableIRQ();

    (void)AM67_disableFIQ();
    int i;
    for (i = 0; i < HwiP_MAX_INTERRUPTS; i++)
    {
        HwiP_setPri(i, 0xF);
        HwiP_setVecAddr((uint32_t)i, (uintptr_t)arm_vectorirq);
    }

    /* disable, clear, set as IRQ and level, all interrupts */
    for (i = 0; i < HwiP_MAX_INTERRUPTS / 32; i++)
    {
        volatile uint32_t *addr;

        /* disable all interrupts */
        addr = (uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_DIS(i * 32));
        *addr = 0xFFFFFFFFu;

        /* clear all pending interrupts */
        addr = (uint32_t *)(gHwiConfig.intcBaseAddr + VIM_STS(i * 32));
        *addr = 0xFFFFFFFFu;

        /* make all as level */
        addr = (uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_TYPE(i * 32));
        *addr = 0x0u;

        /* make all as IRQ */
        addr = (uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_MAP(i * 32));
        *addr = 0x0u;
    }

    /* ACK and clear any pending request */
    {
        gdummy = HwiP_getIRQVecAddr();
        gdummy = HwiP_getFIQVecAddr();
        HwiP_ackIRQ(0);
        HwiP_ackFIQ(0);
    }

    for (i = 0; i < NR_IRQS; i++)
    {
        irq_attach(i, irq_unexpected_isr, NULL);
    }

   // AM67_enableVIC();
    //AM67_enableFIQ();
    up_irq_enable();
}

uint32_t HwiP_getCPSR(void);

uint32_t HwiP_inISR(void)
{
    uint32_t mode = (HwiP_getCPSR() & 0x1FU);
    uint32_t result = 0;
    if (mode != ARMV7R_SYSTEM_MODE)
    {
        result = 1;
    }
    return result;
}

uint32_t *arm_decodeirq(uint32_t *regs)
{
    uint32_t intr_num;
    volatile uint32_t dummy = 1;  // Have to read that, sets other registers

    // AM67_disableIRQ();

    dummy = HwiP_getIRQVecAddr();

    if (HwiP_getIRQ(&intr_num) == 0)  // Success
    {
        regs = arm_doirq(intr_num, regs);
    }
    HwiP_clearInt(intr_num);
    HwiP_ackIRQ(intr_num);

    return regs;
}

void up_disable_irq(int irq) { HwiP_disableInt(irq); }

void up_enable_irq(int irq) { HwiP_enableInt(irq); }
