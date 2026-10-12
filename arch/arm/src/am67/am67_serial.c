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

#include <stdint.h>
#include <nuttx/serial/serial.h>
#include <nuttx/serial/uart_16550.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include "arm_internal.h"
#include "am67_tisci.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

#define AM67_UART_MDR1_OFFSET  0x20   /* MDR1: 0 = UART 16x, 7 = disabled */
#define AM67_UART_DLL_OFFSET   0x00   /* with LCR.DLAB = 1 */
#define AM67_UART_DLH_OFFSET   0x04   /* with LCR.DLAB = 1, 6 bits */
#define AM67_UART_LCR_OFFSET   0x0c
#define AM67_UART_LCR_DLAB     (1u << 7)
#define AM67_UART_MDR1_16X     0u
#define AM67_UART_MDR1_13X     3u
#define AM67_UART_MDR1_DISABLE 7u
#define AM67_UART_DIV_MAX      0x3fffu

#if defined(USE_SERIALDRIVER) /* && defined(HAVE_UART_DEVICE)*/

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
 * Name: open_uart
 *
 * Description:
 *   Initialize UART by clearing the register at offset 0x20 from base.
 *
 ****************************************************************************/

static void open_uart(void)
{
  putreg32(0, CONFIG_16550_UART0_BASE + AM67_UART_MDR1_OFFSET);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_16550_UART1

/****************************************************************************
 * Name: am67_uart6_enable
 *
 * Description:
 *   Power MAIN_UART6 (NuttX UART1) through the DMSC and put it in UART 16x
 *   mode. The px4-r5f overlay keeps Linux off this UART, so nobody else
 *   turns it on, and MDR1 resets to 0x7 (disabled). Must run before the
 *   port is opened.
 *
 ****************************************************************************/

int am67_uart6_enable(void)
{
  int ret = am67_tisci_device_require(AM67_TISCI_DEV_MAIN_UART6);

  if (ret < 0)
    {
      return ret;
    }

  putreg32(0, CONFIG_16550_UART1_BASE + AM67_UART_MDR1_OFFSET);
  return 0;
}
#endif

/****************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   Initialize the serial port by performing early initialization, main
 *   initialization, and opening the UART.
 *
 ****************************************************************************/

void arm_serialinit(void)
{
#if CONFIG_16550_UART0_BASE == 0x2810000
  /* MAIN_UART1: powered by nobody else once Linux leaves it alone (the
   * px4-r5f overlay disables it), and touching an unpowered UART aborts.
   */

  (void)am67_tisci_device_require(AM67_TISCI_DEV_MAIN_UART1);
#endif

  u16550_earlyserialinit();
  u16550_serialinit();
  open_uart();
}

/****************************************************************************
 * Name: am67_uart_divisor
 *
 * Description:
 *   The TI UART takes 16 or 13 clocks per bit (MDR1 = 0 or 3).  The 16550
 *   driver only knows 16x: with the 48 MHz functional clock 460800 and
 *   921600 baud then miss by 7-9 %, beyond what any receiver accepts.
 *   Pick the mode and divisor with the smaller baud error (16x on a tie).
 *
 ****************************************************************************/

static void am67_uart_divisor(uint32_t clk, uint32_t baud,
                              uint32_t *div, uint32_t *mdr1)
{
  static const uint8_t over[2] =
  {
    16, 13
  };

  uint32_t best_err = UINT32_MAX;
  int i;

  *div = 1;
  *mdr1 = AM67_UART_MDR1_16X;

  if (baud == 0)
    {
      return;
    }

  for (i = 0; i < 2; i++)
    {
      uint32_t d = (clk + over[i] * baud / 2) / (over[i] * baud);
      uint32_t actual;
      uint32_t err;

      if (d == 0 || d > AM67_UART_DIV_MAX)
        {
          continue;
        }

      actual = clk / (over[i] * d);
      err = actual > baud ? actual - baud : baud - actual;
      if (err < best_err)
        {
          best_err = err;
          *div = d;
          *mdr1 = i == 0 ? AM67_UART_MDR1_16X : AM67_UART_MDR1_13X;
        }
    }
}

/****************************************************************************
 * Name: uart_getreg
 *
 * Description:
 *   Read a value from the UART register at the specified offset from the
 *   base address.
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
 *   Write a value to the UART register at the specified offset from the
 *   base address.
 *
 ****************************************************************************/

void uart_putreg(FAR struct u16550_s *priv, unsigned int offset,
                 uart_datawidth_t value)
{
  /* u16550_setup() writes the divisor latch DLM, then DLL, with DLAB set.
   * Disable the UART at the first write (TI: change the divisor with
   * MDR1 = 7), and at the second program divisor and over-sampling mode
   * chosen for the configured baud.
   */

  if ((offset == AM67_UART_DLH_OFFSET || offset == AM67_UART_DLL_OFFSET) &&
      (getreg32(priv->uartbase + AM67_UART_LCR_OFFSET) &
       AM67_UART_LCR_DLAB) != 0)
    {
      uint32_t div;
      uint32_t mdr1;

      if (offset == AM67_UART_DLH_OFFSET)
        {
          putreg32(AM67_UART_MDR1_DISABLE,
                   priv->uartbase + AM67_UART_MDR1_OFFSET);
          return;
        }

      am67_uart_divisor(priv->uartclk, priv->baud, &div, &mdr1);
      putreg32(div & 0xff, priv->uartbase + AM67_UART_DLL_OFFSET);
      putreg32((div >> 8) & 0x3f, priv->uartbase + AM67_UART_DLH_OFFSET);
      putreg32(mdr1, priv->uartbase + AM67_UART_MDR1_OFFSET);
      return;
    }

  putreg32(value, priv->uartbase + offset);
}

#endif /* USE_SERIALDRIVER */
