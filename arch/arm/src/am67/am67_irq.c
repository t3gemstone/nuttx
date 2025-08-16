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
 
#include <nuttx/config.h>

#include <assert.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "arm_internal.h"
#include "irq/irq.h"
#include "sctlr.h"

#include "am67_gpio.h"
#include "am67_irq.h"
 
 /****************************************************************************
 * Public Functions
 ****************************************************************************/
 
extern uint8_t _vector_start[]; /* Beginning of vector block */
extern uint8_t _vector_end[];   /* End+1 of vector block */

static volatile uint32_t gdummy;
 
 /****************************************************************************
 * Name: up_irqinitialize
 *
 * Description:
 *   This function is called by up_initialize() during the bring-up of the
 *   system.  It is the responsibility of this function to but the interrupt
 *   subsystem into the working and ready state.
 *
 ****************************************************************************/

void up_irqinitialize(void)
{
    sched_lock(); // Put here to preven the crash in sched_unlock.
                  // May be placed into other function.

#ifdef CONFIG_ARCH_LOWVECTORS
    DEBUGASSERT((((uintptr_t)_vector_start) & ~VBAR_MASK) == 0);
    //cp15_wrvbar((uint32_t)_vector_start); // A crash occurs here
#endif /* CONFIG_ARCH_LOWVECTORS */
     
     irq_init();
 }
  
/****************************************************************************
 * Name: arm_decodeirq
 *
 * Description:
 *   This function is called from the IRQ vector handler in arm_vectors.S.
 *   At this point, the interrupt has been taken and the registers have
 *   been saved on the stack.  This function simply needs to determine the
 *   the irq number of the interrupt and then to call arm_doirq to dispatch
 *   the interrupt.
 *
 *  Input parameters:
 *   regs - A pointer to the register save area on the stack.
 *
 ****************************************************************************/

uint32_t intr_disable(void);

uint32_t *arm_decodeirq(uint32_t *regs)
{
    uint32_t intr_num;
    volatile uint32_t dummy; // Have to read that, sets other registers

    dummy = get_irq_vec_addr();

    if (get_irq(&intr_num) == 0) // Success
    {
        regs = arm_doirq(intr_num, regs);
    }
    
    intr_disable();
    clear_intr(intr_num);
    ack_irq(intr_num);

    return regs;
}


void up_disable_irq(int irq)
{
    disable_intr(irq);
}
 
void up_enable_irq(int irq)
{
    enable_intr(irq);
}

void clear_intr(uint32_t intr_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    addr = (volatile uint32_t *)(INTC_BASE_ADDR + VIM_STS(intr_num));
    bit_pos = VIM_BIT_POS(intr_num);

    *addr = (0x01u << bit_pos);
}

void enable_intr(uint32_t intr_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    addr = (volatile uint32_t *)(INTC_BASE_ADDR + VIM_INT_EN(intr_num));
    bit_pos = VIM_BIT_POS(intr_num);

    *addr = (0x1 << bit_pos);
}

uint32_t disable_intr(uint32_t intr_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;
    uint32_t is_enable = 0;

    addr = (volatile uint32_t *)(INTC_BASE_ADDR + VIM_INT_DIS(intr_num));
    bit_pos = VIM_BIT_POS(intr_num);

    if ((*addr & ((uint32_t)0x1 << bit_pos)) != 0u)
        is_enable = 1;
    *addr = ((uint32_t)0x1 << bit_pos);

    return is_enable;
}

// Enables the interrupts in hardware level
void intr_enable(void)
{
    __asm__ volatile
    (
         "mrs r0, cpsr\n"
         "bic r12, r0, #0x80\n"
         "msr cpsr_cf, r12\n"
         "bx lr\n"
    );
}

// Disables the interrupts in hardware level
uint32_t intr_disable(void)
{
    uint32_t result;
    __asm__ volatile
    (
        "mrs	%0, cpsr\n"
        "orr	r12, %0, #0x80\n"
        "msr	cpsr_cf, r12"
        : "=r" (result)
        :
        : "r12"
    );
    return result;
}

void irq_init(void)
{
    (void)intr_disable(); // Disable IRQ

    for (int i = 0; i < MAX_INTERRUPTS / 32; i++)
    {
        volatile uint32_t *addr;

        // Disable all interrupts
        addr = (uint32_t *)(INTC_BASE_ADDR + VIM_INT_DIS(i * 32));
        *addr = 0xFFFFFFFFu;

        // Clear all pending interrupts
        addr = (uint32_t *)(INTC_BASE_ADDR + VIM_STS(i * 32));
        *addr = 0xFFFFFFFFu;

        // Make all as level
        addr = (uint32_t *)(INTC_BASE_ADDR + VIM_INT_TYPE(i * 32));
        *addr = 0x0u;

        // Make all as IRQ
        addr = (uint32_t *)(INTC_BASE_ADDR + VIM_INT_MAP(i * 32));
        *addr = 0x0u;
    }
  
    // ACK and clear any pending request
    gdummy = get_irq_vec_addr();
    ack_irq(0);

    // Says do not enable irq here
}
