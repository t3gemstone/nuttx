/* Copyright (C) 2021 Texas Instruments Incorporated */
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

#include <nuttx/irq.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HwiP_MAX_INTERRUPTS     (512u)
#define HwiP_MAX_PRIORITY       (16u)

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

#define SystemP_SUCCESS   ((int32_t )0)
#define SystemP_FAILURE   ((int32_t)-1)
#define SystemP_TIMEOUT   ((int32_t)-2)

#define HwiP_OBJECT_SIZE_MAX    (32u)

typedef struct HwiP_Config_
{
    uint32_t intcBaseAddr ; /**< For R5F, this is VIM base addr */

} HwiP_Config;


typedef struct HwiP_Params_ {

    uint32_t intNum;   /**< CPU interrupt number. */
    xcpt_t callback ;
    void *args; /**< Arguments to pass to the callback */
    uint16_t eventId; /**< Event ID to register against, only used with c6x with event combiner and c7x clec configurer  */
    uint8_t priority; /**< Interrupt priority, only used with ARM R5, ARM M4 */
    uint8_t isFIQ; /**< 0: Map interrupt as ISR, 1: map interrupt as FIQ, only used with ARM R5 */
    uint8_t isPulse; /**< 0: Map interrupt as level interrupt, 1: Map interrupt as pulse interrupt, only used with ARM R5, ARM M4 */

} HwiP_Params;

typedef struct HwiP_Object_ {

    uint32_t rsv[HwiP_OBJECT_SIZE_MAX/sizeof(uint32_t)]; /**< reserved, should NOT be modified by end users */

} HwiP_Object;

typedef struct HwiP_Struct_s {

    uint32_t intNum;

} HwiP_Struct;

extern HwiP_Config gHwiConfig;


int HwiP_irq_handler(int irq, FAR void *context, FAR void *arg);
int HwiP_fiq_handler(int irq, FAR void *context, FAR void *arg);
void AM67_irq_init(void);

static inline void  HwiP_setAsFIQ(uint32_t intNum, uint32_t isFIQ)
{
    volatile uint32_t *addr;
    uint32_t bitPos;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_MAP(intNum));
    bitPos = VIM_BIT_POS(intNum);

    if(isFIQ != 0U)
    {
        *addr |= (0x1u << bitPos);
    }
    else
    {
        *addr &= ~(0x1u << bitPos);
    }
}

static inline uint32_t  HwiP_isPulse(uint32_t intNum)
{
    volatile uint32_t *addr;
    uint32_t bitPos;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_TYPE(intNum));
    bitPos = VIM_BIT_POS(intNum);

    return ((*addr >> bitPos) & 0x1u );
}


static inline void  HwiP_setAsPulse(uint32_t intNum, uint32_t isPulse)
{
    volatile uint32_t *addr;
    uint32_t bitPos;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_TYPE(intNum));
    bitPos = VIM_BIT_POS(intNum);

    if(isPulse != 0U)
    {
        *addr |= (0x1u << bitPos);
    }
    else
    {
        *addr &= ~(0x1u << bitPos);
    }
}

static inline void  HwiP_setPri(uint32_t intNum, uint32_t priority)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_PRI(intNum));

    *addr = (priority & 0xFu);
}

static inline void HwiP_setVecAddr(uint32_t intNum, uintptr_t vecAddr)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_INT_VEC(intNum));

    *addr = ((uint32_t)vecAddr & 0xFFFFFFFCU);
}

static inline uint32_t HwiP_getIRQVecAddr(void)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_IRQVEC);

    return *addr;
}

static inline uint32_t HwiP_getFIQVecAddr(void)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_FIQVEC);

    return *addr;
}

static inline int32_t HwiP_getIRQ(uint32_t *intNum)
{
    volatile uint32_t *addr;
    int32_t status = SystemP_FAILURE;
    uint32_t value;

    *intNum = 0;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_ACTIRQ);
    value = *addr;

    if((value & 0x80000000U) != 0U)
    {
        *intNum = (value & (HwiP_MAX_INTERRUPTS-1U));
        status = SystemP_SUCCESS;
    }
    return status;
}

static inline int32_t HwiP_getFIQ(uint32_t *intNum)
{
    volatile uint32_t *addr;
    int32_t status = SystemP_FAILURE;
    uint32_t value;

    *intNum = 0;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_ACTFIQ);
    value = *addr;

    if((value & 0x80000000U) != 0U)
    {
        *intNum = (value & 0x3FFU);
        status = SystemP_SUCCESS;
    }
    return status;
}

static inline void HwiP_ackIRQ(uint32_t intNum)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_IRQVEC);
    *addr= intNum;
}

static inline void HwiP_ackFIQ(uint32_t intNum)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(gHwiConfig.intcBaseAddr + VIM_FIQVEC);
    *addr= intNum;
}

static inline void Utils_dataAndInstructionBarrier(void)
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


static inline void AM67_disableIRQ(void)
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

static inline void AM67_disableFIQ(void) {
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


static inline void AM67_enableFIQ(void)
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


static inline void AM67_enableIRQ(void)
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

static inline void AM67_enableVIC(void)
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


#endif /* __ARCH_ARM_SRC_AM67_AM67_IRQ_H */
