/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_i2c_hw.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_I2C_HW_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_I2C_HW_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Peripheral Base Addresses ************************************************/

#define AM67A_MCU_I2C0_BASE                   (0x04900000u)  /* MCU Domain MCU_I2C0 */
#define AM67A_MCU_WKUP_I2C0_BASE              (0x2b200000u)  /* WKUP Domain WKUP_I2C0 */

/* Register Offsets *********************************************************/

#define AM67A_MCU_I2C_SYSC_OFFSET             (0x0010u)
#define AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET     (0x0024u)
#define AM67A_MCU_I2C_IRQ_STAT_OFFSET         (0x0028u)
#define AM67A_MCU_I2C_IRQ_EN_SET_OFFSET       (0x002cu)
#define AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET       (0x0030u)
#define AM67A_MCU_I2C_WE_OFFSET               (0x0034u)
#define AM67A_MCU_I2C_DMA_RX_EN_SET_OFFSET    (0x0038u)
#define AM67A_MCU_I2C_DMA_TX_EN_SET_OFFSET    (0x003cu)
#define AM67A_MCU_I2C_DMA_RX_EN_CLR_OFFSET    (0x0040u)
#define AM67A_MCU_I2C_DMA_TX_EN_CLR_OFFSET    (0x0044u)
#define AM67A_MCU_I2C_DMA_RX_WAKE_EN_OFFSET   (0x0048u)
#define AM67A_MCU_I2C_DMA_TX_WAKE_EN_OFFSET   (0x004cu)
#define AM67A_MCU_I2C_SYSS_OFFSET             (0x0090u)
#define AM67A_MCU_I2C_BUF_OFFSET              (0x0094u)
#define AM67A_MCU_I2C_CNT_OFFSET              (0x0098u)
#define AM67A_MCU_I2C_DATA_OFFSET             (0x009cu)
#define AM67A_MCU_I2C_CON_OFFSET              (0x00a4u)
#define AM67A_MCU_I2C_OA_OFFSET               (0x00a8u)
#define AM67A_MCU_I2C_SA_OFFSET               (0x00acu)
#define AM67A_MCU_I2C_PSC_OFFSET              (0x00b0u)
#define AM67A_MCU_I2C_SCLL_OFFSET             (0x00b4u)
#define AM67A_MCU_I2C_SCLH_OFFSET             (0x00b8u)
#define AM67A_MCU_I2C_SYSTEST_OFFSET          (0x00bcu)
#define AM67A_MCU_I2C_BUFSTAT_OFFSET          (0x00c0u)
#define AM67A_MCU_I2C_OA1_OFFSET              (0x00c4u)
#define AM67A_MCU_I2C_OA2_OFFSET              (0x00c8u)
#define AM67A_MCU_I2C_OA3_OFFSET              (0x00ccu)
#define AM67A_MCU_I2C_ACTOA_OFFSET            (0x00d0u)
#define AM67A_MCU_I2C_SBLOCK_OFFSET           (0x00d4u)

/* Register Bit Field Definitions *******************************************/

/* SYSC */

#define I2C_SYSC_AUTOIDLE                     (1u << 0)  /* Auto-idle */
#define I2C_SYSC_SRST                         (1u << 1)  /* SoftReset */
#define I2C_SYSC_WAKEUP                       (1u << 2)  /* Enable Wakeup control */
#define I2C_SYSC_IDLE_SHIFT                   (3)        /* Idle Mode selection */
#define I2C_SYSC_IDLE_MASK                    (3u << I2C_SYSC_IDLE_SHIFT)
#  define I2C_SYSC_IDLE_FORCE                 (0u << I2C_SYSC_IDLE_SHIFT) /* Force-idle mode */
#  define I2C_SYSC_IDLE_NO                    (1u << I2C_SYSC_IDLE_SHIFT) /* No-idle mode */
#  define I2C_SYSC_IDLE_SMART                 (2u << I2C_SYSC_IDLE_SHIFT) /* Smart-idle mode */
#  define I2C_SYSC_IDLE_SMART_WKUP            (3u << I2C_SYSC_IDLE_SHIFT) /* Smart-idle Wakeup mode */

#define I2C_SYSC_CLK_SHIFT                    (8)        /* Clock Activity selection */
#define I2C_SYSC_CLK_MASK                     (3u << I2C_SYSC_CLK_SHIFT)
#  define I2C_SYSC_CLK_NONE                   (0u << I2C_SYSC_CLK_SHIFT) /* Both clocks cut off */
#  define I2C_SYSC_CLK_OCP                    (1u << I2C_SYSC_CLK_SHIFT) /* Only Interface/OCP clock active */
#  define I2C_SYSC_CLK_FUNC                   (2u << I2C_SYSC_CLK_SHIFT) /* Only function clock active */
#  define I2C_SYSC_CLK_BOTH                   (3u << I2C_SYSC_CLK_SHIFT) /* Both clocks active */

/* IRQSTATUS / IRQENABLE */

#define I2C_IRQ_AL                            (1u << 0)   /* Arbitration lost */
#define I2C_IRQ_NACK                          (1u << 1)   /* No acknowledgment */
#define I2C_IRQ_ARDY                          (1u << 2)   /* Register access ready */
#define I2C_IRQ_RRDY                          (1u << 3)   /* Receive data ready */
#define I2C_IRQ_XRDY                          (1u << 4)   /* Transmit data ready */
#define I2C_IRQ_GC                            (1u << 5)   /* General call */
#define I2C_IRQ_STC                           (1u << 6)   /* Start Condition */
#define I2C_IRQ_AERR                          (1u << 7)   /* Access Error */
#define I2C_IRQ_BF                            (1u << 8)   /* Bus Free */
#define I2C_IRQ_AAS                           (1u << 9)   /* Address recognized as slave */
#define I2C_IRQ_XUDF                          (1u << 10)  /* Transmit underflow */
#define I2C_IRQ_ROVR                          (1u << 11)  /* Receive overrun */
#define I2C_IRQ_BB                            (1u << 12)  /* Bus busy */
#define I2C_IRQ_RDR                           (1u << 13)  /* Receive draining IRQ */
#define I2C_IRQ_XDR                           (1u << 14)  /* Transmit draining IRQ */

#define I2C_IRQ_ERRORMASK                     (I2C_IRQ_AL | I2C_IRQ_NACK | I2C_IRQ_AERR | \
                                               I2C_IRQ_XUDF | I2C_IRQ_ROVR)

#define I2C_STS_CLEARMASK                     (I2C_IRQ_AL | I2C_IRQ_NACK | I2C_IRQ_ARDY | \
                                               I2C_IRQ_RRDY | I2C_IRQ_XRDY | I2C_IRQ_GC | \
                                               I2C_IRQ_STC | I2C_IRQ_AERR | I2C_IRQ_BF | \
                                               I2C_IRQ_AAS | I2C_IRQ_XUDF | I2C_IRQ_ROVR | \
                                               I2C_IRQ_BB | I2C_IRQ_RDR | I2C_IRQ_XDR)

#define I2C_ICR_CLEARMASK                     (I2C_IRQ_AL | I2C_IRQ_NACK | I2C_IRQ_ARDY | \
                                               I2C_IRQ_RRDY | I2C_IRQ_XRDY | I2C_IRQ_GC | \
                                               I2C_IRQ_STC | I2C_IRQ_AERR | I2C_IRQ_BF | \
                                               I2C_IRQ_AAS | I2C_IRQ_XUDF | I2C_IRQ_ROVR | \
                                               I2C_IRQ_RDR | I2C_IRQ_XDR)

/* WE (Wakeup Enable) */

#define I2C_WE_AL                             (1u << 0)
#define I2C_WE_NACK                           (1u << 1)
#define I2C_WE_ARDY                           (1u << 2)
#define I2C_WE_DRDY                           (1u << 3)
#define I2C_WE_GC                             (1u << 5)
#define I2C_WE_STC                            (1u << 6)
#define I2C_WE_BF                             (1u << 8)
#define I2C_WE_AAS                            (1u << 9)
#define I2C_WE_XUDF                           (1u << 10)
#define I2C_WE_ROVR                           (1u << 11)
#define I2C_WE_RDR                            (1u << 13)
#define I2C_WE_XDR                            (1u << 14)

/* SYSS */

#define I2C_SYSS_RST_DONE                     (1u << 0)  /* Reset done */

/* BUF */

#define I2C_BUF_TXTRSH_SHIFT                  (0)
#define I2C_BUF_TXTRSH_MASK                   (63u << I2C_BUF_TXTRSH_SHIFT)
#define I2C_BUF_TXFIFO_CLR                    (1u << 6)
#define I2C_BUF_XDMA_EN                       (1u << 7)
#define I2C_BUF_RXTRSH_SHIFT                  (8)
#define I2C_BUF_RXTRSH_MASK                   (63u << I2C_BUF_RXTRSH_SHIFT)
#define I2C_BUF_RXFIFO_CLR                    (1u << 14)
#define I2C_BUF_RDMA_EN                       (1u << 15)

/* CON */

#define I2C_CON_STT                           (1u << 0)   /* Start condition */
#define I2C_CON_STP                           (1u << 1)   /* Stop condition */
#define I2C_CON_XOA3                          (1u << 4)
#define I2C_CON_XOA2                          (1u << 5)
#define I2C_CON_XOA1                          (1u << 6)
#define I2C_CON_XOA0                          (1u << 7)
#define I2C_CON_XSA                           (1u << 8)   /* Expand slave address (10-bit) */
#define I2C_CON_TRX                           (1u << 9)   /* Transmitter mode */
#define I2C_CON_MST                           (1u << 10)  /* Master mode */
#define I2C_CON_STB                           (1u << 11)  /* Start byte mode */
#define I2C_CON_OPMODE_SHIFT                  (12)
#define I2C_CON_OPMODE_MASK                   (3u << I2C_CON_OPMODE_SHIFT)
#  define I2C_CON_OPMODE_FAST                 (0u << I2C_CON_OPMODE_SHIFT)
#define I2C_CON_EN                            (1u << 15)  /* I2C module enable */

/* SYSTEST */

#define I2C_SYSTEST_FREE                      (1u << 14)  /* Free running mode */
#define I2C_SYSTEST_ST_EN                     (1u << 15)  /* System test enable */

#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_I2C_HW_H */
