/****************************************************************************
 * arch/arm/src/am67/am67_uart.h
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

/****************************************************************************
* Pre-processor Definitions
****************************************************************************/

#define UART_FIFO_SIZE                  (64U)
#define UART_ERROR_COUNT            (0x00FFFFFFU)
#define UART_TRANSFER_STATUS_SUCCESS         (0U)
#define UART_TRANSFER_STATUS_TIMEOUT         (1U)
#define UART_TRANSFER_STATUS_ERROR_BI        (2U)
#define UART_TRANSFER_STATUS_ERROR_FE        (3U)
#define UART_TRANSFER_STATUS_ERROR_PE        (4U)
#define UART_TRANSFER_STATUS_ERROR_OE        (5U)
#define UART_TRANSFER_STATUS_CANCELLED       (6U)
#define UART_TRANSFER_STATUS_STARTED         (7U)
#define UART_TRANSFER_STATUS_READ_TIMEOUT    (8U)
#define UART_TRANSFER_STATUS_ERROR_INUSE     (9U)
#define UART_TRANSFER_STATUS_ERROR_OTH       (10U)
 
#define UART_TRANSFER_MODE_BLOCKING     (0U)
#define UART_TRANSFER_MODE_CALLBACK     (1U)
#define UART_READ_RETURN_MODE_FULL        (0U)
#define UART_READ_RETURN_MODE_PARTIAL     (1U)

#define UART_LEN_5                      (0U)
#define UART_LEN_6                      (1U)
#define UART_LEN_7                      (2U)
#define UART_LEN_8                      (3U)

#define UART_STOPBITS_1                 (0U)
#define UART_STOPBITS_2                 (1U)

#define UART_PARITY_NONE                (0x00U)
#define UART_PARITY_ODD                 (0x01U)
#define UART_PARITY_EVEN                (0x03U)
#define UART_PARITY_FORCED0             (0x07U)
#define UART_PARITY_FORCED1             (0x05U)

#define UART_FCTYPE_NONE                (0x00U)
#define UART_FCTYPE_HW                  (0x02U)

#define UART_FCPARAM_RXNONE             (0x00U)
#define UART_FCPARAM_RXXONXOFF_2        (0x01U)
#define UART_FCPARAM_RXXONXOFF_1        (0x02U)
#define UART_FCPARAM_RXXONXOFF_12       (0x03U)
#define UART_FCPARAM_AUTO_RTS           (0x40U)

#define UART_FCPARAM_TXNONE             (0x00U)
#define UART_FCPARAM_TXXONXOFF_2        (0x04U)
#define UART_FCPARAM_TXXONXOFF_1        (0x08U)
#define UART_FCPARAM_TXXONXOFF_12       (0x0CU)
#define UART_FCPARAM_AUTO_CTS           (0x80U)

#define UART_RXTRIGLVL_1                (1U)
#define UART_RXTRIGLVL_8                (8U)
#define UART_RXTRIGLVL_16               (16U)
#define UART_RXTRIGLVL_56               (56U)
#define UART_RXTRIGLVL_60               (60U)

#define UART_TXTRIGLVL_1                (1U)
#define UART_TXTRIGLVL_8                (8U)
#define UART_TXTRIGLVL_16               (16U)
#define UART_TXTRIGLVL_32               (32U)
#define UART_TXTRIGLVL_56               (56U)

#define UART_OPER_MODE_16X              (0U)
#define UART_OPER_MODE_SIR              (1U)
#define UART_OPER_MODE_16X_AUTO_BAUD    (2U)
#define UART_OPER_MODE_13X              (3U)
#define UART_OPER_MODE_MIR              (4U)
#define UART_OPER_MODE_FIR              (5U)
#define UART_OPER_MODE_CIR              (6U)
#define UART_OPER_MODE_DISABLED         (7U)

#define UART_TX_FIFO_NOT_FULL               ( \
        UART_SSR_TX_FIFO_FULL_TX_FIFO_FULL_VALUE_0)
#define UART_TX_FIFO_FULL                   ( \
        UART_SSR_TX_FIFO_FULL_TX_FIFO_FULL_VALUE_1)
        
#define UART_INTID_MODEM_STAT               (UART_IIR_IT_TYPE_IT_TYPE_VALUE_0 \
                                             <<                               \
                                             UART_IIR_IT_TYPE_SHIFT)
#define UART_INTID_TX_THRES_REACH           (UART_IIR_IT_TYPE_IT_TYPE_VALUE_1 \
                                             <<                               \
                                             UART_IIR_IT_TYPE_SHIFT)
#define UART_INTID_RX_THRES_REACH           (UART_IIR_IT_TYPE_IT_TYPE_VALUE_2 \
                                             <<                               \
                                             UART_IIR_IT_TYPE_SHIFT)
#define UART_INTID_RX_LINE_STAT_ERROR       (UART_IIR_IT_TYPE_IT_TYPE_VALUE_3 \
                                             <<                               \
                                             UART_IIR_IT_TYPE_SHIFT)
#define UART_INTID_CHAR_TIMEOUT             (UART_IIR_IT_TYPE_IT_TYPE_VALUE_6 \
                                             <<                               \
                                             UART_IIR_IT_TYPE_SHIFT)
#define UART_INTID_XOFF_SPEC_CHAR_DETECT    (UART_IIR_IT_TYPE_IT_TYPE_VALUE_8 \
                                             <<                               \
                                             UART_IIR_IT_TYPE_SHIFT)
#define UART_INTID_MODEM_SIG_STATE_CHANGE   (UART_IIR_IT_TYPE_IT_TYPE_VALUE_10 \
                                             <<                                \
                                             UART_IIR_IT_TYPE_SHIFT)
                                             
#define UART_INTR_PENDING                   (0U)
#define UART_N0_INTR_PENDING                (1U)

#define UART_INTR_CTS                       (UART_IER_CTS_IT_MASK)
#define UART_INTR_RTS                       (UART_IER_RTS_IT_MASK)
#define UART_INTR_XOFF                      (UART_IER_XOFF_IT_MASK)
#define UART_INTR_SLEEPMODE                 (UART_IER_SLEEP_MODE_MASK)
#define UART_INTR_MODEM_STAT                (UART_IER_MODEM_STS_IT_MASK)
#define UART_INTR_LINE_STAT                 (UART_IER_LINE_STS_IT_MASK)
#define UART_INTR_THR                       (UART_IER_THR_IT_MASK)
#define UART_INTR_RHR_CTI                   (UART_IER_RHR_IT_MASK)

#define UART_INTR2_RX_EMPTY                 (UART_IER2_EN_RXFIFO_EMPTY_MASK)
#define UART_INTR2_TX_EMPTY                 (UART_IER2_EN_TXFIFO_EMPTY_MASK)

#define UART_FIFO_PE_FE_BI_DETECTED         (UART_LSR_RX_FIFO_STS_MASK)
#define UART_BREAK_DETECTED_ERROR           (UART_LSR_RX_BI_MASK)
#define UART_FRAMING_ERROR                  (UART_LSR_RX_FE_MASK)
#define UART_PARITY_ERROR                   (UART_LSR_RX_PE_MASK)
#define UART_OVERRUN_ERROR                  (UART_LSR_RX_OE_MASK)

#define UART_REG_CONFIG_MODE_A          ((uint32_t) 0x0080)
#define UART_REG_CONFIG_MODE_B          ((uint32_t) 0x00BF)
#define UART_REG_OPERATIONAL_MODE       ((uint32_t) 0x007F)

#define UART_CONFIG_MODE_POLLED       (0x00U)
#define UART_CONFIG_MODE_INTERRUPT    (0x01U)
#define UART_CONFIG_MODE_USER_INTR    (0x02U)
#define UART_CONFIG_MODE_DMA          (0x03U)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions Prototypes
 ****************************************************************************/
 
void uart_init(void);
