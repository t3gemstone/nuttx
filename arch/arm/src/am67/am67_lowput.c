/****************************************************************************
 * arch/arm/src/am67/am67_lowputc.c
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

#include <stdint.h>
#include <arch/board/board.h>

#include "arm_internal.h"
#include "hardware/am67_uart.h"
#include "am67_pinmux.h"
#include "am67_lowput.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_lowputc
 *
 * Description:
 *   Output one byte on the serial console
 *
 ****************************************************************************/
void arm_lowputc(char ch)
{
    /* Wait for the transmitter to be available */

    while ((getreg32(CONSOLE_BASE + AM67_UART_LSR_UART_OFFSET) &
                UART_LSR_THRE) == 0)
    {
    }

    /* Send the character */

    putreg32((uint32_t)ch, CONSOLE_BASE + AM67_UART_THR_OFFSET);
    while ((getreg32(CONSOLE_BASE + AM67_UART_LSR_UART_OFFSET) &
                UART_LSR_THRE) == 0)
    {
    }
}

/****************************************************************************
 * Name: am67_lowsetup
 *
 * Description:
 *   This performs basic initialization of the UART used for the serial
 *   console.  Its purpose is to get the console output available as soon
 *   as possible.
 *
 ****************************************************************************/
void am67_lowsetup(void)
{
    pinmux_init();
}


