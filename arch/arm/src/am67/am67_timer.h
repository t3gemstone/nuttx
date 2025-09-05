/* Copyright (C) 2021 Texas Instruments Incorporated */
/****************************************************************************
 * arch/arm/src/am67/am67_pinmux.h
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
 
#ifndef __ARCH_ARM_SRC_AM67_AM67_TIMER_H
#define __ARCH_ARM_SRC_AM67_AM67_TIMER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define TIMER0_CLOCK_SRC_MUX_ADDR		(0x1081B0u)
#define TIMER0_CLOCK_SRC_HFOSC0_CLKOUT		(0x0u)
#define TIMER0_BASE_ADDR			(0x2400000u)


#define AM67_DMTIMER1_1MS_TIMER0_VADDR          0x2400000

#define AM67_DMTMR1MS_TIDR_OFFSET               0x0000 /* Identification Register Section */
#define AM67_DMTMR1MS_TIOCP_CFG_OFFSET          0x0010 /* 1ms Timer OCP Configuration Register Section */
#define AM67_DMTMR1MS_IRQ_EOI_OFFSET            0x0020 /* 1ms Timer IRQ Wakeup Enable Register */
#define AM67_DMTMR1MS_IRQSTATUS_RAW_OFFSET      0x0024 /* 1ms Timer IRQ Status Register */
#define AM67_DMTMR1MS_IRQSTATUS_OFFSET          0x0028 /* 1ms Timer IRQ Enable Register */
#define AM67_DMTMR1MS_IRQSTATUS_SET_OFFSET      0x002C /* 1ms Timer IRQ Enable Register */
#define AM67_DMTMR1MS_IRQSTATUS_CLR_OFFSET      0x0030 /* 1ms Timer IRQ Enable Register */
#define AM67_DMTMR1MS_IRQWAKEEN_OFFSET          0x0034 /* 1ms Timer IRQ Enable Register */
#define AM67_DMTMR1MS_TCLR_OFFSET               0x0038 /* 1ms Timer Control Register */
#define AM67_DMTMR1MS_TCRR_OFFSET               0x003C /* 1ms Timer Counter Register */
#define AM67_DMTMR1MS_TLDR_OFFSET               0x0040 /* 1ms Timer Load Register */
#define AM67_DMTMR1MS_TTGR_OFFSET               0x0044 /* 1ms Timer Trigger Register */
#define AM67_DMTMR1MS_TWPS_OFFSET               0x0048 /* 1ms Timer Write Posting Bits Register */
#define AM67_DMTMR1MS_TMAR_OFFSET               0x004C /* 1ms Timer Match Register */
#define AM67_DMTMR1MS_TCAR1_OFFSET              0x0050 /* 1ms Timer Capture 1 Register */
#define AM67_DMTMR1MS_TSICR_OFFSET              0x0054 /* 1ms Timer Synchronous Interface Control Register */
#define AM67_DMTMR1MS_TCAR2_OFFSET              0x0058 /* 1ms Timer Capture 2 Register */
#define AM67_DMTMR1MS_TPIR_OFFSET               0x005C /* 1ms Timer Positive Increment Register */
#define AM67_DMTMR1MS_TNIR_OFFSET               0x0060 /* 1ms Timer Negative Increment Register */
#define AM67_DMTMR1MS_TCVR_OFFSET               0x0064 /* 1ms Timer Counter Value Register */
#define AM67_DMTMR1MS_TOCR_OFFSET               0x0068 /* 1ms Timer Overflow Counter Register */
#define AM67_DMTMR1MS_TOWR_OFFSET               0x006C /* 1ms Timer Overflow Interrupts Register */

#define TIMER_OVF_INT_SHIFT     (0x1)

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef struct Timer_Params_ {

    uint32_t inputPreScaler; /**< input pre-scaler divisor ro apply
                              *
                              * \note MUST be power of 2 and between 1 and 256 for GP Timer
                              * \note MAKE sure this value is not 0
                              * \note This field is valid only when underlying timer is DM Timer.
                              * \note This field is not valid when underlying timer is RTI Timer. Set to 1 in this case.
                              */
    uint32_t inputClkHz;  /**< Timer input clock in unit of Hz before pre-scaler, system initialization MUST
                            * make any system level muxes, PLLs, power required to input this clock are setup properly
                            *
                            * \note MAKE sure this value is not 0
                            */
    uint32_t periodInUsec; /**< Timer period in units of usecs, internally \ref TimerP_Params.inputClkHz
                            * and TimerP_Params.inputPreScaler is used to compute the value to be put inside the timer HW register
                            *
                            * \note When value is 0, \ref periodInNsec is used instead
                            * \note When both \ref periodInUsec and \ref periodInNsec are non-zero, \ref periodInNsec is used
                            */
    uint32_t periodInNsec; /**< Timer period in units of nsecs, internally \ref TimerP_Params.inputClkHz
                            * and TimerP_Params.inputPreScaler is used to compute the value to be put inside the timer HW register
                            *
                            * \note When value is 0, \ref periodInUsec is used instead
                            * \note When both \ref periodInUsec and \ref periodInNsec are non-zero, \ref periodInNsec is used
                            */
    uint32_t oneshotMode; /**< 0: continuous mode of operation, 1: oneshot mode of operation
                            *
                            * \note NOT supported for RTI timer, always set to 0 in this case.
                            */
    uint32_t enableOverflowInt; /**< 0: Do not enable timer overflow interrupt, 1: enable timer overflow interrupt */
    uint32_t enableDmaTrigger;  /**< 0: Do not enable DMA trigger from timer, 1: enable DMA trigger from timer */

} Timer_Params;
/****************************************************************************
 * Public Functions
 ****************************************************************************/


/* Function declarations */

void up_timer_initialize(void);

int timer_tick_isr(int irq, void *context, void *arg);

int up_timer_gettime(struct timespec *ts);

int up_timer_start(const struct timespec *ts);

int up_timer_cancel(struct timespec *ts);

void TimerP_Params_init(Timer_Params *params);

void TimerP_setup(uint32_t baseAddr, Timer_Params *params);

void TimerP_start(uint32_t baseAddr);

void TimerP_stop(uint32_t baseAddr);

uint32_t TimerP_getCount(uint32_t baseAddr);

uint32_t TimerP_getReloadCount(uint32_t baseAddr);

void TimerP_clearOverflowInt(uint32_t baseAddr);

uint32_t TimerP_isOverflowed(uint32_t baseAddr);


#endif /* __ARCH_ARM_SRC_AM67_AM67_TIMER_H */
