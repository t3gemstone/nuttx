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
 
 #include <arm_internal.h>
 
 #include "am67_gpio.h"
 #include "am67_irq.h"
 
 /****************************************************************************
 * Public Functions
 ****************************************************************************/
 
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
 
 
 uint32_t *arm_decodeirq(uint32_t *regs)
 {
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

void __attribute__((section(".text.hwi"))) irq_handler_c(void)
{
   uint32_t intr_num;

   if (get_irq(&intr_num) == 0) // Success
   {
       fxn_callback isr;
       void *args;

       isr = gintr_ctrl.isr[intr_num];
       args = gintr_ctrl.isr_args[intr_num];

       if (isr != NULL)
       {
           isr(args);       // Call the callback
       }

       intr_disable();
       clear_intr(intr_num);
       ack_irq(intr_num);
   }
}

void irq_handler(void)
{
    __asm__ volatile
    (
        "SUB lr, lr, #4\n"
        "PUSH {lr}\n"
        "MRS lr, SPSR\n"
        "PUSH {lr}\n"
        "CPS #0x13\n"
        "PUSH {r0-r4, r12}\n"

        // There was some FPU configuration related additions here. Ignored.


        "MOV r2, sp\n"
        "AND r2, r2, #4\n"
        "SUB sp, sp, r2\n"

        "PUSH {r0-r4, lr}\n"
        "LDR r1, =irq_handler_const\n"
        "BLX r1\n"
        "POP {r0-r4, lr}\n"
        "ADD sp, sp, r2\n"

        "CPSID i\n"
        "DSB\n"
        "ISB\n"

        "POP {r0-r4, r12}\n"
        "CPS #0x12\n"
        "POP {LR}\n"
        "MSR SPSR_cxsf, LR\n"
        "POP {LR}\n"
        "MOVS PC, LR\n"
        "irq_handler_const: .word irq_handler_c\n"
    );
}

void irq_init(void)
{
    (void)intr_disable(); // Disable IRQ

    // Initialize the structure, set all interrupts to
    // lowest priority and set ISR address as IRQ handler

    for (int i = 0; i < MAX_INTERRUPTS; i++)
    {
        gintr_ctrl.isr[i] = NULL;
        gintr_ctrl.isr_args[i] = NULL;

        intr_set_priority(i, 0xF);
        set_vector((uint32_t)i, (uintptr_t)irq_handler);
    }

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
    {
        ack_irq(0);
    }

    // Says do not enable irq here
}
