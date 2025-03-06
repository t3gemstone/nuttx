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

#include <arch/irq.h>
#include <arch/board/board.h>

#include "arm_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/* SCLK is the UART input clock.
 * TRM Info
 */
#define AM67_SCLK 48000000

#define CONSOLE_BAUD 115200

/* The output baud rate is equal to the serial clock (SCLK) frequency
 * divided by sixteen times the value of the baud rate divisor, as follows:
 *
 * baud rate = Fsclk / (16 * divisor).
 */
#define CONSOLE_DL (AM67_SCLK / (CONSOLE_BAUD << 4))

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
}
