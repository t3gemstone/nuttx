/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_serial.c
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
#include <nuttx/serial/serial.h>
#include <nuttx/serial/uart_16550.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include "arm_internal.h"
#include "am67a_mcu_rsc_table.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if defined(USE_SERIALDRIVER)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: open_uart
 *
 * Description:
 *   Initialize UART by clearing the MDR1 register at offset 0x20.
 *
 ****************************************************************************/

static void open_uart(void)
{
#ifdef CONFIG_16550_UART0
  putreg32(0, CONFIG_16550_UART0_BASE + 0x20);
#endif
#ifdef CONFIG_16550_UART1
  putreg32(0, CONFIG_16550_UART1_BASE + 0x20);
#endif
#ifdef CONFIG_16550_UART2
  putreg32(0, CONFIG_16550_UART2_BASE + 0x20);
#endif
#ifdef CONFIG_16550_UART3
  putreg32(0, CONFIG_16550_UART3_BASE + 0x20);
#endif
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   Initialize standard 16550 UART port for MCU domain console.
 *
 ****************************************************************************/

void arm_serialinit(void)
{
  u16550_earlyserialinit();
  u16550_serialinit();
  open_uart();
}

/****************************************************************************
 * Name: uart_getreg
 *
 * Description:
 *   Read register at specified offset from UART base address.
 *
 ****************************************************************************/

uart_datawidth_t uart_getreg(FAR struct u16550_s *priv, unsigned int offset)
{
  return getreg32(priv->uartbase + offset);
}

/****************************************************************************
 * Name: uart_putreg
 *
 * Description:
 *   Write register at specified offset from UART base address.
 *   Transmitted characters (offset 0) are also mirrored to the RemoteProc
 *   trace buffer.
 *
 ****************************************************************************/

void uart_putreg(FAR struct u16550_s *priv, unsigned int offset,
                 uart_datawidth_t value)
{
  if (offset == 0)
    {
      am67a_mcu_trace_putc((int)value);
    }

  putreg32(value, priv->uartbase + offset);

  /* TI K3 / AM67A 16550 UART hardware requirement:
   * When baud rate divisor (DLL/DLM) or FIFO configuration is modified,
   * MDR1 (offset 0x20) must be programmed to 0x00 (16x UART Mode) to start
   * the internal baud rate generator. u16550_setup clears DLAB by writing
   * LCR (offset 0x0c) and then configures FCR (offset 0x08).
   */

  if (offset == 0x08 || offset == 0x0c)
    {
      putreg32(0, priv->uartbase + 0x20);
    }
}

#endif /* USE_SERIALDRIVER */

