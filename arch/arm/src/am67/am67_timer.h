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

#define TIMER_IRQ_EOI           (0x20u)
#define TIMER_IRQ_STATUS_RAW    (0x24u)
#define TIMER_IRQ_STATUS        (0x28u)
#define TIMER_IRQ_INT_ENABLE    (0x2Cu)
#define TIMER_IRQ_INT_DISABLE   (0x30u)
#define TIMER_TCLR_OFFSET	(0x38u)
#define TIMER_TCRR_OFFSET	(0x3Cu)
#define TIMER_TLDR_OFFSET	(0x40u)

#define TIMER_OVF_INT_SHIFT     (0x1)

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct timer
{
    uint32_t clock_hz;
    uint32_t prescaler;
    uint32_t period_usec;
    uint32_t one_shot;      	// oneshotmode, runs continuously
    uint32_t overflow_intr;     // Interrup occurs when timer overflows
    uint32_t dma;
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void timer_setup(uint32_t base_addr, struct timer *params);
void timer_start(uint32_t base_addr);
void timer_stop(uint32_t base_addr);
uint32_t get_count(uint32_t base_addr);
uint32_t get_reload(uint32_t base_addr);
void clear_overflow_int(uint32_t base_addr);

#endif /* __ARCH_ARM_SRC_AM67_AM67_TIMER_H */
