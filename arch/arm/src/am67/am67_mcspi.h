/****************************************************************************
 * arch/arm/src/am67/am67_mcspi.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_MCSPI_H
#define __ARCH_ARM_SRC_AM67_AM67_MCSPI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

#include <nuttx/spi/spi.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MCU_MCSPI0 on Gemstone O1 (MCU domain) */

#define AM67_MCSPI0_BASE           0x40300000u
#define AM67_MCSPI0_IRQ            207

/* OMAP2-compatible register map (K3 MCU_MCSPI0) */

#define AM67_MCSPI_REVISION        0x00
#define AM67_MCSPI_SYSCONFIG       0x10
#define AM67_MCSPI_SYSSTATUS       0x14
#define AM67_MCSPI_IRQSTATUS       0x18
#define AM67_MCSPI_IRQENABLE       0x1c
#define AM67_MCSPI_WAKEUPENABLE    0x20
#define AM67_MCSPI_SYST            0x24
#define AM67_MCSPI_MODULCTRL       0x28
#define AM67_MCSPI_CHCONF0         0x2c
#define AM67_MCSPI_CHSTAT0         0x30
#define AM67_MCSPI_CHCTRL0         0x34
#define AM67_MCSPI_TX0             0x38
#define AM67_MCSPI_RX0             0x3c

#define AM67_MCSPI_CH_OFFSET(n)    ((uint32_t)(n) * 0x14u)
#define AM67_MCSPI_CHCONF(b, n)    ((b) + AM67_MCSPI_CH_OFFSET(n) + AM67_MCSPI_CHCONF0)
#define AM67_MCSPI_CHSTAT(b, n)    ((b) + AM67_MCSPI_CH_OFFSET(n) + AM67_MCSPI_CHSTAT0)
#define AM67_MCSPI_CHCTRL(b, n)    ((b) + AM67_MCSPI_CH_OFFSET(n) + AM67_MCSPI_CHCTRL0)
#define AM67_MCSPI_TX(b, n)        ((b) + AM67_MCSPI_CH_OFFSET(n) + AM67_MCSPI_TX0)
#define AM67_MCSPI_RX(b, n)        ((b) + AM67_MCSPI_CH_OFFSET(n) + AM67_MCSPI_RX0)

#define AM67_MCSPI_MODULCTRL_SINGLE (1u << 0)
#define AM67_MCSPI_MODULCTRL_MS     (1u << 2)

#define AM67_MCSPI_CHCONF_PHA       (1u << 0)
#define AM67_MCSPI_CHCONF_POL       (1u << 1)
#define AM67_MCSPI_CHCONF_CLKD_SHIFT 2
#define AM67_MCSPI_CHCONF_CLKD_MASK (0x0fu << AM67_MCSPI_CHCONF_CLKD_SHIFT)
#define AM67_MCSPI_CHCONF_EPOL      (1u << 6)
#define AM67_MCSPI_CHCONF_WL_SHIFT  7
#define AM67_MCSPI_CHCONF_WL_MASK   (0x1fu << AM67_MCSPI_CHCONF_WL_SHIFT)
#define AM67_MCSPI_CHCONF_TRM_RX    (1u << 12)
#define AM67_MCSPI_CHCONF_TRM_TX    (1u << 13)
#define AM67_MCSPI_CHCONF_DPE0      (1u << 16)
#define AM67_MCSPI_CHCONF_DPE1      (1u << 17)
#define AM67_MCSPI_CHCONF_IS        (1u << 18)
#define AM67_MCSPI_CHCONF_FORCE     (1u << 20)
#define AM67_MCSPI_CHCONF_CLKG      (1u << 29)

#define AM67_MCSPI_CHSTAT_RXS       (1u << 0)
#define AM67_MCSPI_CHSTAT_TXS       (1u << 1)
#define AM67_MCSPI_CHSTAT_EOT       (1u << 2)

#define AM67_MCSPI_CHCTRL_EN        (1u << 0)
#define AM67_MCSPI_CHCTRL_EXTCLK_SHIFT 8
#define AM67_MCSPI_CHCTRL_EXTCLK_MASK  (0xffu << AM67_MCSPI_CHCTRL_EXTCLK_SHIFT)

/* Functional clock after Linux/ROM init (MCU_PLL0 path); divider applied in driver */

#ifndef CONFIG_AM67_MCSPI0_FCLK
#  define CONFIG_AM67_MCSPI0_FCLK  48000000
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void am67_spiinitialize(void);
FAR struct spi_dev_s *am67_spibus_initialize(int port);
void am67_mcspi_board_select(FAR struct spi_dev_s *dev, uint32_t devid,
                             bool selected);

#endif /* __ARCH_ARM_SRC_AM67_AM67_MCSPI_H */
