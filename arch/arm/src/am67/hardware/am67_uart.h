/****************************************************************************
 * arch/arm/src/am67/hardware/am67_uart.h
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

#ifndef __ARCH_ARM_SRC_AM67_HARDWARE_AM67_UART_H
#define __ARCH_ARM_SRC_AM67_HARDWARE_AM67_UART_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "am67_memmap.h"

/****************************************************************************
 * Pre-processor Definitions 
 ****************************************************************************/

#define AM67_MCU_UART0_BASE MCU_UART0


/* UART register offsets */

#define AM67_UART_DLL_OFFSET 0x00 /* Divisor Latches Low Register */
#define AM67_UART_RHR_OFFSET 0x00 /* Receive  Holding Register */
#define AM67_UART_THR_OFFSET 0x00 /* Transmit Holding Register */
#define AM67_UART_DATA_BUFFER_OFFSET 0x00 /* [0:7] bits are for data */
#define AM67_UART_DLH_OFFSET 0x04
#define AM67_UART_IER_OFFSET 0x04 /* Interrupt Enable Register */
#define AM67_UART_LCR_OFFSET 0x0C

/* ECR - Enhanced control register 
 * Bit 4: Enable TX, 1 EN, 0 DIS
 * Bit 3: Enable RX, 1 EN, 0 DIS
 */
#define AM67_UART_ECR_OFFSET 0x90 

#endif /* __ARCH_ARM_SRC_AM67_HARDWARE_AM67_UART_H */
