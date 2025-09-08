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
#include "am67_gpio.h"
#include "am67_irq.h"
#include "am67_timer.h"
#include "irq/irq.h"
#include "sctlr.h"

/* TODO: fill that function according to TI implementation */

static volatile uint32_t gdummy;

intr_config_t intr_config;

extern int irq_unexpected_isr(int irq, FAR void *context, FAR void *arg);
extern uint32_t *arm_doirq(int irq, uint32_t *regs);

void intr_enable_int(uint32_t int_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    utils_data_and_instruction_barrier();

    addr = (volatile uint32_t *)(intr_config.intc_base_addr + VIM_INT_EN(int_num));
    bit_pos = VIM_BIT_POS(int_num);

    *addr = (0x1u << bit_pos);
}

uint32_t intr_disable_int(uint32_t int_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;
    uint32_t is_enable = 0;

    addr = (volatile uint32_t *)(intr_config.intc_base_addr + VIM_INT_DIS(int_num));
    bit_pos = VIM_BIT_POS(int_num);

    if ((*addr & ((uint32_t)0x1 << bit_pos)) != 0U)
    {
        is_enable = 1;
    }
    *addr = ((uint32_t)0x1 << bit_pos);

    utils_data_and_instruction_barrier();

    return is_enable;
}

void intr_restore_int(uint32_t int_num, uint32_t old_state)
{
    if (old_state != 0U)
    {
        intr_enable_int(int_num);
    }
    else
    {
        (void)intr_disable_int(int_num);
    }
}

void intr_clear_int(uint32_t int_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    addr = (volatile uint32_t *)(intr_config.intc_base_addr + VIM_STS(int_num));
    bit_pos = VIM_BIT_POS(int_num);

    *addr = (0x1u << bit_pos);
}

void intr_post(uint32_t int_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    addr = (volatile uint32_t *)(intr_config.intc_base_addr + VIM_RAW(int_num));
    bit_pos = VIM_BIT_POS(int_num);

    *addr = (0x1u << bit_pos);
     
    utils_data_and_instruction_barrier();
}

void up_irqinitialize(void)
{
    sched_lock();
    am67_irq_init();
    sched_unlock();
}

void am67_irq_init(void)
{
    intr_config.intc_base_addr = 0x2fff0000u;

    (void)am67_disable_irq();

    (void)am67_disable_fiq();
    int i;
    for (i = 0; i < INTR_MAX_INTERRUPTS; i++)
    {
        intr_setPri(i, 0xF);
        intr_setVecAddr((uint32_t)i, (uintptr_t)arm_vectorirq);
    }

    /* disable, clear, set as IRQ and level, all interrupts */
    for (i = 0; i < INTR_MAX_INTERRUPTS / 32; i++)
    {
        volatile uint32_t *addr;

        /* disable all interrupts */
        addr = (uint32_t *)(intr_config.intc_base_addr + VIM_INT_DIS(i * 32));
        *addr = 0xFFFFFFFFu;

        /* clear all pending interrupts */
        addr = (uint32_t *)(intr_config.intc_base_addr + VIM_STS(i * 32));
        *addr = 0xFFFFFFFFu;

        /* make all as level */
        addr = (uint32_t *)(intr_config.intc_base_addr + VIM_INT_TYPE(i * 32));
        *addr = 0x0u;

        /* make all as IRQ */
        addr = (uint32_t *)(intr_config.intc_base_addr + VIM_INT_MAP(i * 32));
        *addr = 0x0u;
    }

    /* ACK and clear any pending request */
    {
        gdummy = intr_get_irqVecAddr();
        gdummy = intr_get_fiqVecAddr();
        intr_ack_irq(0);
        intr_ack_fiq(0);
    }

    for (i = 0; i < NR_IRQS; i++)
    {
        irq_attach(i, irq_unexpected_isr, NULL);
    }

   /* am67_enable_vic();*/    /*AM67_enable_fiq();*/
   up_irq_enable();
}

uint32_t intr_getCPSR(void);

uint32_t intr_inISR(void)
{
    uint32_t mode = (intr_getCPSR() & 0x1FU);
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
    volatile uint32_t dummy = 1;  /* Have to read that, sets other registers*/

    /* am67_disable_irq();*/
    dummy = intr_get_irqVecAddr();

    if (intr_get_irq(&intr_num) == 0)  /* Success*/
    {
        regs = arm_doirq(intr_num, regs);
    }
    intr_clear_int(intr_num);
    intr_ack_irq(intr_num);

    return regs;
}

void up_disable_irq(int irq) { intr_disable_int(irq); }

void up_enable_irq(int irq) { intr_enable_int(irq); }
