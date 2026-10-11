/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_lowputc.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_LOWPUTC_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_LOWPUTC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MCU UART0 Base Address (TI MCU+ SDK: CSL_MCU_UART0_BASE) */

#define AM67A_MCU_MCU_UART0_BASE              (0x04a00000ul)

/* UART Register Offsets */

#define AM67A_MCU_UART_RHR_OFFSET             (0x00)  /* Receive Holding Register */
#define AM67A_MCU_UART_THR_OFFSET             (0x00)  /* Transmit Holding Register */
#define AM67A_MCU_UART_DLL_OFFSET             (0x00)  /* Divisor Latch Low (DLAB=1) */
#define AM67A_MCU_UART_IER_OFFSET             (0x04)  /* Interrupt Enable Register */
#define AM67A_MCU_UART_DLH_OFFSET             (0x04)  /* Divisor Latch High (DLAB=1) */
#define AM67A_MCU_UART_FCR_OFFSET             (0x08)  /* FIFO Control Register */
#define AM67A_MCU_UART_IIR_OFFSET             (0x08)  /* Interrupt Identification */
#define AM67A_MCU_UART_LCR_OFFSET             (0x0c)  /* Line Control Register */
#define AM67A_MCU_UART_MCR_OFFSET             (0x10)  /* Modem Control Register */
#define AM67A_MCU_UART_LSR_OFFSET             (0x14)  /* Line Status Register */
#define AM67A_MCU_UART_MSR_OFFSET             (0x18)  /* Modem Status Register */
#define AM67A_MCU_UART_MDR1_OFFSET            (0x20)  /* Mode Definition Register 1 */

/* Register Bit Definitions */

#define AM67A_MCU_UART_LCR_DLAB               (1 << 7) /* Divisor Latch Access */
#define AM67A_MCU_UART_LCR_8N1                (0x03)   /* 8 data bits, 1 stop bit, no parity */

#define AM67A_MCU_UART_LSR_THRE               (1 << 5) /* Transmit Holding Register Empty */
#define AM67A_MCU_UART_LSR_TEMT               (1 << 6) /* Transmitter Empty */

#define AM67A_MCU_UART_FCR_FIFO_EN            (1 << 0) /* FIFO Enable */
#define AM67A_MCU_UART_FCR_RXSRST             (1 << 1) /* Receiver Soft Reset */
#define AM67A_MCU_UART_FCR_TXSRST             (1 << 2) /* Transmitter Soft Reset */

#define AM67A_MCU_UART_MDR1_MODE_16X          (0x00)   /* 16x over-sampling */
#define AM67A_MCU_UART_MDR1_MODE_DISABLE      (0x07)   /* Disable mode */

/* Clock and Baud Rate */

#define AM67A_MCU_UART_FREQUENCY              (48000000ul)
#define AM67A_MCU_UART_DEFAULT_BAUD           (115200ul)

/* Divisor = 48000000 / (16 * 115200) = 26 (0x001a) */

#define AM67A_MCU_UART_DIVISOR_115200         (26)

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: am67a_mcu_lowsetup
 *
 * Description:
 *   Initialize MCU UART0 hardware for low-level console output.
 *
 ****************************************************************************/

void am67a_mcu_lowsetup(void);

/****************************************************************************
 * Name: am67a_mcu_lowputc_puts
 *
 * Description:
 *   Output a null-terminated string using up_lowputc.
 *
 ****************************************************************************/

void am67a_mcu_lowputc_puts(const char *str);

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_LOWPUTC_H */
