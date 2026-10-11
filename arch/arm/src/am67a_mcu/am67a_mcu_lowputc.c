/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_lowputc.c
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
#include <nuttx/serial/uart_16550.h>

#include "arm_internal.h"
#include "am67a_mcu_lowputc.h"
#include "am67a_mcu_rsc_table.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UART_REG(o)                       (AM67A_MCU_MCU_UART0_BASE + (o))
#define UART_TIMEOUT_COUNT                (1000000)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_uart_wait_thre
 *
 * Description:
 *   Wait for Transmitter Holding Register Empty (THRE) with timeout.
 *
 ****************************************************************************/

static inline void am67a_mcu_uart_wait_thre(void)
{
  volatile uint32_t timeout = UART_TIMEOUT_COUNT;

  while ((getreg32(UART_REG(AM67A_MCU_UART_LSR_OFFSET)) &
          AM67A_MCU_UART_LSR_THRE) == 0 && timeout > 0)
    {
      timeout--;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_lowsetup
 *
 * Description:
 *   Initialize MCU UART0 hardware (115200 8N1) for early serial output.
 *
 ****************************************************************************/

void am67a_mcu_lowsetup(void)
{
#ifdef CONFIG_16550_UART
  /* 1. Disable UART mode before divisor configuration */

  putreg32(AM67A_MCU_UART_MDR1_MODE_DISABLE,
           UART_REG(AM67A_MCU_UART_MDR1_OFFSET));

  /* 2. Disable interrupts */

  putreg32(0x00, UART_REG(AM67A_MCU_UART_IER_OFFSET));

  /* 3. Enable FIFO, clear TX/RX FIFOs */

  putreg32(AM67A_MCU_UART_FCR_FIFO_EN |
           AM67A_MCU_UART_FCR_RXSRST  |
           AM67A_MCU_UART_FCR_TXSRST,
           UART_REG(AM67A_MCU_UART_FCR_OFFSET));

  /* 4. Enable Divisor Latch Access (DLAB) */

  putreg32(AM67A_MCU_UART_LCR_DLAB | AM67A_MCU_UART_LCR_8N1,
           UART_REG(AM67A_MCU_UART_LCR_OFFSET));

  /* 5. Set baud rate divisor for 115200 (48 MHz / (16 * 115200) = 26) */

  putreg32(AM67A_MCU_UART_DIVISOR_115200 & 0xff,
           UART_REG(AM67A_MCU_UART_DLL_OFFSET));
  putreg32((AM67A_MCU_UART_DIVISOR_115200 >> 8) & 0xff,
           UART_REG(AM67A_MCU_UART_DLH_OFFSET));

  /* 6. Switch back to normal register access with 8N1 framing */

  putreg32(AM67A_MCU_UART_LCR_8N1, UART_REG(AM67A_MCU_UART_LCR_OFFSET));

  /* 7. Switch to 16x over-sampling mode (UART functional mode) */

  putreg32(AM67A_MCU_UART_MDR1_MODE_16X,
           UART_REG(AM67A_MCU_UART_MDR1_OFFSET));
#endif
}

/****************************************************************************
 * Name: up_lowputc
 *
 * Description:
 *   Output one character to Linux RemoteProc Trace Buffer and optional
 *   hardware MCU UART0.
 *
 ****************************************************************************/

void up_lowputc(int ch)
{
  /* 1. Write character to Linux remoteproc trace buffer */

  am67a_mcu_trace_putc(ch);

#ifdef CONFIG_16550_UART
  /* 2. Write character to MCU UART0 hardware */

  am67a_mcu_uart_wait_thre();
  putreg32((uint32_t)ch, UART_REG(AM67A_MCU_UART_THR_OFFSET));
#endif
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Output one character (standard NuttX interface).
 *
 ****************************************************************************/

#ifndef HAVE_16550_CONSOLE
void up_putc(int ch)
{
  if (ch == '\n')
    {
      up_lowputc('\r');
    }

  up_lowputc((char)ch);
}
#endif

/****************************************************************************
 * Name: am67a_mcu_lowputc_puts
 *
 * Description:
 *   Output a null-terminated string using early serial output.
 *
 ****************************************************************************/

void am67a_mcu_lowputc_puts(const char *str)
{
  while (*str != '\0')
    {
      if (*str == '\n')
        {
          up_lowputc('\r');
        }

      up_lowputc((int)*str);
      str++;
    }
}
