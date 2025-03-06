/****************************************************************************
 * arch/arm/src/am67/am67_serial.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/serial/serial.h>

#include <arch/board/board.h>

#include "arm_internal.h"
#include "chip.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

/* If we are not using the serial driver for the console, then we still must
 * provide some minimal implementation of up_putc.
 */

#if defined(USE_SERIALDRIVER) && defined(HAVE_UART_DEVICE)

/* TODO: Move to hardware/uart.h */
#define AM67_SCLK 48000000

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct up_dev_s
{
  uint32_t uartbase;  /* Base address of UART registers */
  uint32_t baud;      /* Configured baud */
  uint32_t ier;       /* Saved IER value */
  uint8_t  irq;       /* IRQ associated with this UART */
  uint8_t  parity;    /* 0=none, 1=odd, 2=even */
  uint8_t  bits;      /* Number of bits (7 or 8) */
  bool     stopbits2; /* true: Configure with 2 stop bits instead of 1 */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  up_setup(struct uart_dev_s *dev);
static void up_shutdown(struct uart_dev_s *dev);
static int  up_attach(struct uart_dev_s *dev);
static void up_detach(struct uart_dev_s *dev);
static int  uart_interrupt(int irq, void *context, void *arg);
static int  up_ioctl(struct file *filep, int cmd, unsigned long arg);
static int  up_receive(struct uart_dev_s *dev, unsigned int *status);
static void up_rxint(struct uart_dev_s *dev, bool enable);
static bool up_rxavailable(struct uart_dev_s *dev);
static void up_send(struct uart_dev_s *dev, int ch);
static void up_txint(struct uart_dev_s *dev, bool enable);
static bool up_txready(struct uart_dev_s *dev);
static bool up_txempty(struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/
/* Which UART with be tty0/console and which tty1-5?  The console will always
 * be ttyS0.  If there is no console then will use the lowest numbered UART.
 */

/* First pick the console and ttys0.  This could be any of UART0-5 */

#if defined(CONFIG_UART0_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart0port /* UART0 is console */
#    define TTYS0_DEV           g_uart0port /* UART0 is ttyS0 */
#    define UART0_ASSIGNED      1
#elif defined(CONFIG_UART1_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart1port /* UART1 is console */
#    define TTYS0_DEV           g_uart1port /* UART1 is ttyS0 */
#    define UART1_ASSIGNED      1
#elif defined(CONFIG_UART2_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart2port /* UART2 is console */
#    define TTYS0_DEV           g_uart2port /* UART2 is ttyS0 */
#    define UART2_ASSIGNED      1
#elif defined(CONFIG_UART3_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart3port /* UART3 is console */
#    define TTYS0_DEV           g_uart3port /* UART3 is ttyS0 */
#    define UART3_ASSIGNED      1
#elif defined(CONFIG_UART4_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart4port /* UART4 is console */
#    define TTYS0_DEV           g_uart4port /* UART4 is ttyS0 */
#    define UART4_ASSIGNED      1
#elif defined(CONFIG_UART5_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart5port /* UART5 is console */
#    define TTYS5_DEV           g_uart5port /* UART5 is ttyS0 */
#    define UART5_ASSIGNED      1
#else
#  undef CONSOLE_DEV                        /* No console */
#  if defined(CONFIG_AM67_UART0)
#    define TTYS0_DEV           g_uart0port /* UART0 is ttyS0 */
#    define UART0_ASSIGNED      1
#  elif defined(CONFIG_AM67_UART1)
#    define TTYS0_DEV           g_uart1port /* UART1 is ttyS0 */
#    define UART1_ASSIGNED      1
#  elif defined(CONFIG_AM67_UART2)
#    define TTYS0_DEV           g_uart2port /* UART2 is ttyS0 */
#    define UART2_ASSIGNED      1
#  elif defined(CONFIG_AM67_UART3)
#    define TTYS0_DEV           g_uart3port /* UART3 is ttyS0 */
#    define UART3_ASSIGNED      1
#  elif defined(CONFIG_AM67_UART4)
#    define TTYS0_DEV           g_uart4port /* UART4 is ttyS0 */
#    define UART4_ASSIGNED      1
#  elif defined(CONFIG_AM67_UART5)
#    define TTYS0_DEV           g_uart5port /* UART5 is ttyS0 */
#    define UART5_ASSIGNED      1
#  endif
#endif

/* Pick ttys1.  This could be any of UART0-5 excluding the console UART. */

#if defined(CONFIG_AM67_UART0) && !defined(UART0_ASSIGNED)
#  define TTYS1_DEV           g_uart0port /* UART0 is ttyS1 */
#  define UART0_ASSIGNED      1
#elif defined(CONFIG_AM67_UART1) && !defined(UART1_ASSIGNED)
#  define TTYS1_DEV           g_uart1port /* UART1 is ttyS1 */
#  define UART1_ASSIGNED      1
#elif defined(CONFIG_AM67_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS1_DEV           g_uart2port /* UART2 is ttyS1 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_AM67_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS1_DEV           g_uart3port /* UART3 is ttyS1 */
#  define UART3_ASSIGNED      1
#elif defined(CONFIG_AM67_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS1_DEV           g_uart4port /* UART4 is ttyS1 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_AM67_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS1_DEV           g_uart5port /* UART5 is ttyS1 */
#  define UART5_ASSIGNED      1
#endif

/* Pick ttys2.  This could be one of UART1-5. It can't be UART0 because that
 * was either assigned as ttyS0 or ttys1.  One of UART 1-5 could also be the
 * console.
 */

#if defined(CONFIG_AM67_UART1) && !defined(UART1_ASSIGNED)
#  define TTYS2_DEV           g_uart1port /* UART1 is ttyS2 */
#  define UART1_ASSIGNED      1
#elif defined(CONFIG_AM67_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS2_DEV           g_uart2port /* UART2 is ttyS2 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_AM67_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS2_DEV           g_uart3port /* UART3 is ttyS2 */
#  define UART3_ASSIGNED      1
#elif defined(CONFIG_AM67_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS2_DEV           g_uart4port /* UART4 is ttyS2 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_AM67_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS2_DEV           g_uart5port /* UART5 is ttyS2 */
#  define UART5_ASSIGNED      1
#endif

/* Pick ttys3. This could be one of UART2-5. It can't be UART0-1 because
 * those have already been assigned to ttsyS0, 1, or 2.  One of
 * UART 2-5 could also be the console.
 */

#if defined(CONFIG_AM67_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS3_DEV           g_uart2port /* UART2 is ttyS3 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_AM67_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS3_DEV           g_uart3port /* UART3 is ttyS3 */
#  define UART3_ASSIGNED      1
#elif defined(CONFIG_AM67_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS3_DEV           g_uart4port /* UART4 is ttyS3 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_AM67_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS3_DEV           g_uart5port /* UART5 is ttyS3 */
#  define UART5_ASSIGNED      1
#endif

/* Pick ttys4. This could be one of UART3-5. It can't be UART0-2 because
 * those have already been assigned to ttsyS0, 1, 2 or 3.  One of
 * UART 3-5 could also be the console.
 */

#if defined(CONFIG_AM67_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS4_DEV           g_uart3port /* UART3 is ttyS4 */
#  define UART3_ASSIGNED      1
#elif defined(CONFIG_AM67_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS4_DEV           g_uart4port /* UART4 is ttyS4 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_AM67_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS4_DEV           g_uart5port /* UART5 is ttyS4 */
#  define UART5_ASSIGNED      1
#endif

/* Pick ttys5. This could be one of UART4-5. It can't be UART0-3 because
 * those have already been assigned to ttsyS0, 1, 2, 3 or 4.  One of
 * UART 4-5 could also be the console.
 */

#if defined(CONFIG_AM67_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS5_DEV           g_uart4port /* UART4 is ttyS5 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_AM67_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS5_DEV           g_uart5port /* UART5 is ttyS5 */
#  define UART5_ASSIGNED      1
#endif

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef USE_EARLYSERIALINIT

/****************************************************************************
 * Name: arm_earlyserialinit
 *
 * Description:
 *   Performs the low level UART initialization early in debug so that the
 *   serial console will be available during bootup.  This must be called
 *   before arm_serialinit.
 *
 *   NOTE: Configuration of the CONSOLE UART was performed by up_lowsetup()
 *   very early in the boot sequence.
 *
 ****************************************************************************/

void arm_earlyserialinit(void)
{
  /* Configure all UARTs (except the CONSOLE UART) and disable interrupts */

#ifdef CONFIG_AM67_UART0
#ifndef CONFIG_UART0_SERIAL_CONSOLE
  am67_uart0config();
#endif
  up_disableuartint(&g_uart0priv, NULL);
#endif

#ifdef CONFIG_AM67_UART1
#ifndef CONFIG_UART1_SERIAL_CONSOLE
  am67_uart1config();
#else
#endif
  up_disableuartint(&g_uart1priv, NULL);
#endif

#ifdef CONFIG_AM67_UART2
#ifndef CONFIG_UART2_SERIAL_CONSOLE
  am67_uart2config();
#endif
  up_disableuartint(&g_uart2priv, NULL);
#endif

#ifdef CONFIG_AM67_UART3
#ifndef CONFIG_UART3_SERIAL_CONSOLE
  am67_uart3config();
#endif
  up_disableuartint(&g_uart3priv, NULL);
#endif

#ifdef CONFIG_AM67_UART4
#ifndef CONFIG_UART4_SERIAL_CONSOLE
  am67_uart4config();
#endif
  up_disableuartint(&g_uart4priv, NULL);
#endif

#ifdef CONFIG_AM67_UART5
#ifndef CONFIG_UART5_SERIAL_CONSOLE
  am67_uart5config();
#endif
  up_disableuartint(&g_uart5priv, NULL);
#endif

  /* Configuration whichever one is the console */

#ifdef CONSOLE_DEV
  CONSOLE_DEV.isconsole = true;
  up_setup(&CONSOLE_DEV);
#endif
}
#endif

/****************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   Register serial console and serial ports.  This assumes that
 *   arm_earlyserialinit was called previously.
 *
 ****************************************************************************/

void arm_serialinit(void)
{
#ifdef CONSOLE_DEV
  uart_register("/dev/console", &CONSOLE_DEV);
#endif
#ifdef TTYS0_DEV
  uart_register("/dev/ttyS0", &TTYS0_DEV);
#endif
#ifdef TTYS1_DEV
  uart_register("/dev/ttyS1", &TTYS1_DEV);
#endif
#ifdef TTYS2_DEV
  uart_register("/dev/ttyS2", &TTYS2_DEV);
#endif
#ifdef TTYS3_DEV
  uart_register("/dev/ttyS3", &TTYS3_DEV);
#endif
#ifdef TTYS4_DEV
  uart_register("/dev/ttyS4", &TTYS4_DEV);
#endif
#ifdef TTYS5_DEV
  uart_register("/dev/ttyS5", &TTYS5_DEV);
#endif
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug  writes
 *
 ****************************************************************************/

void up_putc(int ch)
{
#ifdef HAVE_SERIAL_CONSOLE
  struct up_dev_s *priv = (struct up_dev_s *)CONSOLE_DEV.priv;
  uint32_t ier;
  up_disableuartint(priv, &ier);
#endif

  arm_lowputc(ch);
#ifdef HAVE_SERIAL_CONSOLE
  up_restoreuartint(priv, ier);
#endif
}

#else /* USE_SERIALDRIVER */

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug writes
 *
 ****************************************************************************/

void up_putc(int ch)
{
#ifdef HAVE_UART_DEVICE
  arm_lowputc(ch);
#endif
}

#endif /* USE_SERIALDRIVER */
