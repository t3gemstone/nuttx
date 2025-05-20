/****************************************************************************
 * arch/arm/src/am67/am67_serial.h
 *
 * Author: Emre CECANPUNAR <emreleno@gmail.com>
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_SERIAL_H
#define __ARCH_ARM_SRC_AM67_AM67_SERIAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <arch/board/board.h>

#include "hardware/am67_uart.h"
#include <nuttx/serial/uart_16550.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* IRQ for AM67 UART */
/* Page 1150(MAIN) - 1182(R5F) in Techincal Reference Manual document for AM67 */

#define AM67_UART0_IRQ       210         /* AM67 UART0 IRQ */
#define AM67_UART1_IRQ       211         /* AM67 UART1 IRQ */
#define AM67_UART2_IRQ       212         /* AM67 UART2 IRQ */
#define AM67_UART3_IRQ       213         /* AM67 UART3 IRQ */
#define AM67_UART4_IRQ       214         /* AM67 UART4 IRQ */
#define AM67_UART5_IRQ       215         /* AM67 UART5 IRQ */
#define AM67_UART6_IRQ       216         /* AM67 UART6 IRQ */

#undef USE_SERIALDRIVER

#endif /* __ARCH_ARM_SRC_AM67_AM67_SERIAL_H */
