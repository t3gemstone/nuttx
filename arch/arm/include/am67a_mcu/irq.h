/****************************************************************************
 * arch/arm/include/am67a_mcu/irq.h
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

/* This file should never be included directly but, rather,
 * only indirectly through nuttx/irq.h
 */

#ifndef __ARCH_ARM_INCLUDE_AM67A_MCU_IRQ_H
#define __ARCH_ARM_INCLUDE_AM67A_MCU_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MCU Domain Cortex-R5F Interrupt Definitions
 * Sourced from TI MCU+ SDK: cslr_intr_mcu_r5fss0_core0.h
 */

#define AM67A_MCU_IRQ_MCU_TIMER0              (28)
#define AM67A_MCU_IRQ_MCU_TIMER1              (29)
#define AM67A_MCU_IRQ_MCU_RTI0_WWD            (30)
#define AM67A_MCU_IRQ_I2C0                    (197)
#define AM67A_MCU_IRQ_UART0                   (210)
#define AM67A_MCU_IRQ_MCU_UART0               (217)
#define AM67A_MCU_IRQ_WKUP_UART0              (219)

/* Total number of Vectored Interrupt Manager (VIM) IRQs */

#define NR_IRQS                               (256)

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM_INCLUDE_AM67A_MCU_IRQ_H */
