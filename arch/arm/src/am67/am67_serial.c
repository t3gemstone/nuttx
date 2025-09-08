/****************************************************************************
 * arch/arm/src/am67/am67_serial.c
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

#include <string.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/serial/serial.h>

#include <nuttx/serial/uart_16550.h>

#include "arm_internal.h"
#include "chip.h"
#include "am67_serial.h"
#include "am67_lowput.h"
#include "am67_pinmux.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

#if defined(USE_SERIALDRIVER) // && defined(HAVE_UART_DEVICE)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void arm_serialinit(void)
{
    
    u16550_earlyserialinit();
    u16550_serialinit();
    open_uart();
}

uart_datawidth_t uart_getreg(FAR struct u16550_s *priv, unsigned int offset)
{
    volatile uint32_t *reg = (volatile uint32_t *)(priv->uartbase + offset);
    return *reg;
}

void open_uart(){
    volatile uint32_t *reg = (volatile uint32_t *)(0x0002800000 + 0x20);
    *reg = 0;
}

void uart_putreg(FAR struct u16550_s *priv, unsigned int offset, uart_datawidth_t value)
{
    volatile uint32_t *reg = (volatile uint32_t *)(priv->uartbase + offset);
    *reg = value;
}

#endif /* USE_SERIALDRIVER */
