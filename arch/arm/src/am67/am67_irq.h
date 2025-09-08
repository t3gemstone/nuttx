/****************************************************************************
 * arch/arm/src/am67/am67_irq.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_IRQ_H
#define __ARCH_ARM_SRC_AM67_AM67_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/irq.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define INTR_MAX_INTERRUPTS     (512u)
#define intr_MAX_PRIORITY       (16u)
#define INTRC_BASE_ADDR         0x2fff0000u

#define VIM_BIT_POS(j)   ( (j) & 0x1Fu )
#define VIM_IRQVEC       (0x18u)
#define VIM_FIQVEC       (0x1Cu)
#define VIM_ACTIRQ       (0x20u)
#define VIM_ACTFIQ       (0x24u)
#define VIM_RAW(j)       (0x400U + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_STS(j)       (0x404U + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_EN(j)    (0x408U + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_DIS(j)   (0x40CU + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_MAP(j)   (0x418U + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_TYPE(j)  (0x41CU + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_PRI(j)   (0x1000u + ((j) * 0x4u))
#define VIM_INT_VEC(j)   (0x2000u + ((j) * 0x4u))

#define ARMV7R_FIQ_MODE       (0x11u)
#define ARMV7R_IRQ_MODE       (0x12u)
#define ARMV7R_SVC_MODE       (0x13u)
#define ARMV7R_SYSTEM_MODE    (0x1Fu)

#define INTR_SUCCESS   ((int32_t )0)
#define INTR_FAILURE   ((int32_t)-1)
#define INTR_TIMEOUT   ((int32_t)-2)


typedef struct intr_Params_ {

     /*CPU interrupt number. */
    uint32_t int_num;  
    xcpt_t callback ;
    void *args; /* Arguments to pass to the callback */
    uint16_t event_id; /* Event ID to register against, only used with c6x with event combiner and c7x clec configurer  */
    uint8_t priority; /* Interrupt priority, only used with ARM R5, ARM M4 */
    uint8_t is_fiq; /* 0: Map interrupt as ISR, 1: map interrupt as FIQ, only used with ARM R5 */
    uint8_t is_pulse; /* 0: Map interrupt as level interrupt, 1: Map interrupt as pulse interrupt, only used with ARM R5, ARM M4 */

} intr_Params;



typedef struct intr_Struct_s {

    uint32_t int_num;

} intr_Struct;


int intr_irq_handler(int irq, FAR void *context, FAR void *arg);
int intr_fiq_handler(int irq, FAR void *context, FAR void *arg);
void am67_irq_init(void);


/****************************************************************************
 * Name: intr_set_as_fiq
 * Description
 * Converts interrupts to Fast Interrupt
 ****************************************************************************/
static inline void  intr_set_as_fiq(uint32_t int_num, uint32_t is_fiq)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_INT_MAP(int_num));
    bit_pos = VIM_BIT_POS(int_num);

    if(is_fiq != 0U)
    {
        *addr |= (0x1u << bit_pos);
    }
    else
    {
        *addr &= ~(0x1u << bit_pos);
    }
}



/****************************************************************************
 * Name: intr_set_pri
 * Description
 * sets the priority level (0–15) for a specified interrupt 
 ****************************************************************************/
static inline void  intr_set_pri(uint32_t int_num, uint32_t priority)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_INT_PRI(int_num));

    *addr = (priority & 0xFu);
}

/****************************************************************************
 * Name: intr_setVecAddr
 * Description
 * sets the interrupt vector address for a specific interrupt number into the corresponding interrupt controller register.
 ****************************************************************************/
static inline void intr_setVecAddr(uint32_t int_num, uintptr_t vecAddr)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_INT_VEC(int_num));

    *addr = ((uint32_t)vecAddr & 0xFFFFFFFCU);
}

/****************************************************************************
 * Name: intr_get_irq_vec_addr
 * Description
 * gets the interrupt vector address for a specific interrupt number from the corresponding interrupt controller register.
 ****************************************************************************/
static inline uint32_t intr_get_irq_vec_addr(void)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_IRQVEC);

    return *addr;
}

/****************************************************************************
 * Name: intr_get_fiq_vec_addr
 * Description
 * gets the fast interrupt vector address for a specific interrupt number from the corresponding interrupt controller register.
 ****************************************************************************/
static inline uint32_t intr_get_fiq_vec_addr(void)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_FIQVEC);

    return *addr;
}

/****************************************************************************
 * Name: intr_get_irq
 * Description
 * Returns the interrupt status corresponding to the given int_num.
 ****************************************************************************/
static inline int32_t intr_get_irq(uint32_t *int_num)
{
    volatile uint32_t *addr;
    int32_t status = INTR_FAILURE;
    uint32_t value;

    *int_num = 0;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_ACTIRQ);
    value = *addr;

    if((value & 0x80000000U) != 0U)
    {
        *int_num = (value & (INTR_MAX_INTERRUPTS-1U));
        status = INTR_SUCCESS;
    }
    return status;
}

/****************************************************************************
 * Name: intr_get_fiq
 * Description
 * Returns the fast interrupt status corresponding to the given int_num.
 ****************************************************************************/
static inline int32_t intr_get_fiq(uint32_t *int_num)
{
    volatile uint32_t *addr;
    int32_t status = INTR_FAILURE;
    uint32_t value;

    *int_num = 0;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_ACTFIQ);
    value = *addr;

    if((value & 0x80000000U) != 0U)
    {
        *int_num = (value & 0x3FFU);
        status = INTR_SUCCESS;
    }
    return status;
}

/****************************************************************************
 * Name: intr_ack_irq
 ****************************************************************************/
static inline void intr_ack_irq(uint32_t int_num)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_IRQVEC);
    *addr = int_num;
}

/****************************************************************************
 * Name: utils_data_and_instruction_barrier
 * Description 
 * Enforces CPU memory ordering by executing an Instruction Synchronization Barrier (ISB) followed by a Data Synchronization Barrier (DSB),
 * ensuring all previous instructions complete and memory accesses are synchronized before continuing execution.
 ****************************************************************************/
static inline void utils_data_and_instruction_barrier(void)
{
    __asm__ __volatile__(
        " isb"
        "\n\t"
        :
        :
        : "memory");
    __asm__ __volatile__(
        " dsb"
        "\n\t"
        :
        :
        : "memory");
}


/****************************************************************************
 * Name: am67_disable_irq
 * Description
 * Disables interrupt request in assembly level
 ****************************************************************************/
static inline void am67_disable_irq(void)
{
    unsigned int cpsr;
    asm volatile(
        "mrs %0, cpsr\n"
        "orr %0, %0, #0x80\n"
        "msr cpsr_c, %0"
        : "=r" (cpsr)
        :
        : "memory"
    );
}

/****************************************************************************
 * Name: am67_disable_fiq
 * Description
 * Disables interrupt request in assembly level
 ****************************************************************************/
static inline void am67_disable_fiq(void) {
    unsigned int cpsr;
    __asm__ volatile (
        "mrs %0, cpsr\n\t"           /* Read CPSR into cpsr variable*/
        "orr %0, %0, #0x40\n\t"      /* Set the FIQ disable bit (bit 6)*/
        "msr cpsr_cf, %0"            /* Write back modified CPSR (control flags)*/
        : "=r" (cpsr)
        :
        : "memory"
    );
}


/****************************************************************************
 * Name: am67_enable_fiq
 * Description
 * Enables fast interrupt request in assembly level
 ****************************************************************************/
static inline void am67_enable_fiq(void)
{
    unsigned int cpsr;
    asm volatile(
        "mrs %0, cpsr\n"
        "bic %0, %0, #0x40\n"
        "msr cpsr_c, %0"
        : "=r" (cpsr)
        :
        : "memory"
    );
}


/****************************************************************************
 * Name: am67_enable_irq
 * Description
 * Enables interrupt request in assembly level
 ****************************************************************************/
static inline void am67_enable_irq(void)
{
    unsigned int cpsr;
    asm volatile(
        "mrs %0, cpsr\n"
        "bic %0, %0, #0x80\n"
        "msr cpsr_c, %0"
        : "=r" (cpsr)
        :
        : "memory"
    );
}

/****************************************************************************
 * Name: am67_enable_vic
 * Description
 * Enables Virtual Interrupt Controller in assembly level
 ****************************************************************************/
static inline void am67_enable_vic(void)
{
    unsigned int sctlr;
    asm volatile(
        "MRC p15, 0, %0, c1, c0, 0\n"
        "ORR %0, %0, #(1 << 24)\n"
        "MCR p15, 0, %0, c1, c0, 0"
        : "=r" (sctlr)
        :
        : "memory"
    );
}


/****************************************************************************
 * Name: intr_is_pulse
 ****************************************************************************/
static inline uint32_t  intr_is_pulse(uint32_t int_num)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_INT_TYPE(int_num));
    bit_pos = VIM_BIT_POS(int_num);

    return ((*addr >> bit_pos) & 0x1u );
}


/****************************************************************************
 * Name: intr_set_as_pulse
 ****************************************************************************/
static inline void  intr_set_as_pulse(uint32_t int_num, uint32_t is_pulse)
{
    volatile uint32_t *addr;
    uint32_t bit_pos;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_INT_TYPE(int_num));
    bit_pos = VIM_BIT_POS(int_num);

    if(is_pulse != 0U)
    {
        *addr |= (0x1u << bit_pos);
    }
    else
    {
        *addr &= ~(0x1u << bit_pos);
    }
}

/****************************************************************************
 * Name: intr_ack_fiq
 ****************************************************************************/
static inline void intr_ack_fiq(uint32_t int_num)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(INTRC_BASE_ADDR + VIM_FIQVEC);
    *addr= int_num;
}


#endif /* __ARCH_ARM_SRC_AM67_AM67_IRQ_H */
