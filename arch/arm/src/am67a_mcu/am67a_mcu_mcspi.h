/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_mcspi.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_MCSPI_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_MCSPI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

#include <nuttx/spi/spi.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MCU Domain MCSPI Instance Base Addresses & IRQs
 * (TI MCU+ SDK: CSL_MCU_MCSPIx_CFG_BASE)
 */

#define AM67A_MCU_MCU_MCSPI0_BASE       (0x04b00000u)
#define AM67A_MCU_MCU_MCSPI0_IRQ        (207)

#define AM67A_MCU_MCU_MCSPI1_BASE       (0x04b10000u)
#define AM67A_MCU_MCU_MCSPI1_IRQ        (208)

#define AM67A_MCU_MCSPI_MAX_CHANNELS    (4u)

/* K3 MCSPI v0 register map (TRM offsets).
 * The peripheral prepends an HL header block (HL_REV/HL_HWINFO/HL_SYSCONFIG)
 * at 0x000-0x01f; functional registers start at 0x100.
 */

/* HL header registers (0x000-0x01f) */

#define AM67A_MCU_MCSPI_HL_SYSCONFIG    (0x010u)

/* HL_SYSCONFIG bits */

#define AM67A_MCU_MCSPI_HL_SYSCONFIG_NOIDLE         (1u << 2)

/* Functional registers start at 0x100 */

#define AM67A_MCU_MCSPI_REVISION        (0x100u)
#define AM67A_MCU_MCSPI_SYSCONFIG       (0x110u)
#define AM67A_MCU_MCSPI_SYSSTATUS       (0x114u)
#define AM67A_MCU_MCSPI_IRQSTATUS       (0x118u)
#define AM67A_MCU_MCSPI_IRQENABLE       (0x11cu)
#define AM67A_MCU_MCSPI_WAKEUPENABLE    (0x120u)
#define AM67A_MCU_MCSPI_SYST            (0x124u)
#define AM67A_MCU_MCSPI_MODULCTRL       (0x128u)
#define AM67A_MCU_MCSPI_CHCONF0         (0x12cu)
#define AM67A_MCU_MCSPI_CHSTAT0         (0x130u)
#define AM67A_MCU_MCSPI_CHCTRL0         (0x134u)
#define AM67A_MCU_MCSPI_TX0             (0x138u)
#define AM67A_MCU_MCSPI_RX0             (0x13cu)

#define AM67A_MCU_MCSPI_CH_OFFSET(n)    ((uint32_t)(n) * 0x14u)

/* SYSCONFIG bit definitions */

#define AM67A_MCU_MCSPI_SYSCONFIG_AUTOIDLE          (1u << 0)
#define AM67A_MCU_MCSPI_SYSCONFIG_SOFTRESET         (1u << 1)
#define AM67A_MCU_MCSPI_SYSCONFIG_SIDLEMODE_SMART   (2u << 3)
#define AM67A_MCU_MCSPI_SYSCONFIG_SIDLEMODE_NO      (1u << 3)
#define AM67A_MCU_MCSPI_SYSCONFIG_CLKACT_BOTH       (3u << 8)

/* SYSSTATUS bit definitions */

#define AM67A_MCU_MCSPI_SYSSTATUS_RESETDONE         (1u << 0)

/* MODULCTRL bit definitions */

#define AM67A_MCU_MCSPI_MODULCTRL_SINGLE            (1u << 0)
#define AM67A_MCU_MCSPI_MODULCTRL_MS                (1u << 2)

/* CHCONF bit definitions */

#define AM67A_MCU_MCSPI_CHCONF_PHA                  (1u << 0)
#define AM67A_MCU_MCSPI_CHCONF_POL                  (1u << 1)
#define AM67A_MCU_MCSPI_CHCONF_CLKD_SHIFT           (2)
#define AM67A_MCU_MCSPI_CHCONF_CLKD_MASK            (0x0fu << AM67A_MCU_MCSPI_CHCONF_CLKD_SHIFT)
#define AM67A_MCU_MCSPI_CHCONF_EPOL                 (1u << 6)
#define AM67A_MCU_MCSPI_CHCONF_WL_SHIFT             (7)
#define AM67A_MCU_MCSPI_CHCONF_WL_MASK              (0x1fu << AM67A_MCU_MCSPI_CHCONF_WL_SHIFT)
#define AM67A_MCU_MCSPI_CHCONF_TRM_RX               (1u << 12)
#define AM67A_MCU_MCSPI_CHCONF_TRM_TX               (1u << 13)
#define AM67A_MCU_MCSPI_CHCONF_DPE0                 (1u << 16)
#define AM67A_MCU_MCSPI_CHCONF_DPE1                 (1u << 17)
#define AM67A_MCU_MCSPI_CHCONF_IS                   (1u << 18)
#define AM67A_MCU_MCSPI_CHCONF_FORCE                (1u << 20)
#define AM67A_MCU_MCSPI_CHCONF_SPIENSLV_SHIFT       (21)
#define AM67A_MCU_MCSPI_CHCONF_SPIENSLV_MASK        (3u << AM67A_MCU_MCSPI_CHCONF_SPIENSLV_SHIFT)
#define AM67A_MCU_MCSPI_CHCONF_TCS_SHIFT            (25)
#define AM67A_MCU_MCSPI_CHCONF_TCS_MASK             (3u << AM67A_MCU_MCSPI_CHCONF_TCS_SHIFT)
#define AM67A_MCU_MCSPI_CHCONF_TCS_2P5CYCLES        (3u << AM67A_MCU_MCSPI_CHCONF_TCS_SHIFT)
#define AM67A_MCU_MCSPI_CHCONF_CLKG                 (1u << 29)

/* CHSTAT bit definitions */

#define AM67A_MCU_MCSPI_CHSTAT_RXS                  (1u << 0)
#define AM67A_MCU_MCSPI_CHSTAT_TXS                  (1u << 1)
#define AM67A_MCU_MCSPI_CHSTAT_EOT                  (1u << 2)

/* CHCTRL bit definitions */

#define AM67A_MCU_MCSPI_CHCTRL_EN                   (1u << 0)
#define AM67A_MCU_MCSPI_CHCTRL_EXTCLK_SHIFT         (8)
#define AM67A_MCU_MCSPI_CHCTRL_EXTCLK_MASK          (0xffu << AM67A_MCU_MCSPI_CHCTRL_EXTCLK_SHIFT)

/* Functional reference clock (50 MHz from MCU_PLL0_HSDIVOUT6 / 8) */

#ifndef CONFIG_AM67A_MCU_MCSPI_FCLK
#  define CONFIG_AM67A_MCU_MCSPI_FCLK           (50000000u)
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: am67a_mcu_spiinitialize
 *
 * Description:
 *   Initialize the selected MCSPI bus controller(s) and pin multiplexing.
 *
 ****************************************************************************/

void am67a_mcu_spiinitialize(void);

/****************************************************************************
 * Name: am67a_mcu_spibus_initialize
 *
 * Description:
 *   Initialize the selected SPI bus port and return the standard SPI device
 *   interface structure pointer.
 *
 * Input Parameters:
 *   port - SPI port number (0 for MCU_MCSPI0, 1 for MCU_MCSPI1)
 *
 * Returned Value:
 *   Valid pointer to struct spi_dev_s on success; NULL on failure.
 *
 ****************************************************************************/

FAR struct spi_dev_s *am67a_mcu_spibus_initialize(int port);

/****************************************************************************
 * Name: am67a_mcu_mcspi_board_select
 *
 * Description:
 *   Board-level chip select control function.
 *
 ****************************************************************************/

void am67a_mcu_mcspi_board_select(FAR struct spi_dev_s *dev, uint8_t channel,
                              bool selected);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_MCSPI_H */
