/****************************************************************************
 * arch/arm/src/am67/am67_pinmux.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_PINMUX_H
#define __ARCH_ARM_SRC_AM67_AM67_PINMUX_H
#endif

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__

#include <stdint.h>

#endif /* __ASSEMBLY__ */

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CSL_PADCFG_CTRL0_CFG0_BASE      (0xf0000UL) // If in the main domain
#define CSL_PADCFG_CTRL0_CFG0_SIZE      (0x8000UL)

#define CSL_MCU_PADCFG_CTRL0_CFG0_BASE  (0x4080000UL)
#define CSL_MCU_PADCFG_CTRL0_CFG0_SIZE  (0x8000UL)

#define PADCFG_PMUX_OFFSET              (0x4000U)

#define CSL_MAIN_PADCONFIG_LOCK0_KICK0_OFFSET   (0x1008)
#define CSL_MAIN_PADCONFIG_LOCK1_KICK0_OFFSET   (0x5008)
#define CSL_MCU_PADCONFIG_LOCK0_KICK0_OFFSET    (0x1008)
#define CSL_MCU_PADCONFIG_LOCK1_KICK0_OFFSET    (0x5008)

#define KICK_LOCK_VAL                   (0x00000000U)
#define KICK0_UNLOCK_VAL                (0x68EF3490U)
#define KICK1_UNLOCK_VAL                (0xD172BC5AU)

#define PINMUX_END				(-1)

#define PIN_MODE(mode)				((uint32_t) mode)
#define PIN_PULL_DISABLE			(((uint32_t) 0x1U) << 16U)
#define PIN_PULL_DIRECTION			(((uint32_t) 0x1U) << 17U)
#define PIN_INPUT_ENABLE			(((uint32_t) 0x1U) << 18U)
#define PIN_OUTPUT_DISABLE			(((uint32_t) 0x1U) << 21U)
#define PIN_WAKEUP_ENABLE			(((uint32_t) 0x1U) << 29U)

enum Pinmux_MainOffsets
{
    PIN_MMC1_DAT1		 = 0x022C,
	PIN_MMC1_DAT0		 = 0x0230,
	PIN_EXT_REFCLK1		 = 0x01F0,
	PIN_MMC1_DAT3		 = 0x0224,
	PIN_MMC1_DAT2		 = 0x0228,
	PIN_VOUT0_VSYNC		 = 0x0100,
	PIN_VOUT0_HSYNC		 = 0x00F8,
	PIN_VOUT0_PCLK		 = 0x0104,
	PIN_VOUT0_DE		 = 0x00FC,
	PIN_VOUT0_DATA0		 = 0x00B8,
	PIN_VOUT0_DATA1		 = 0x00BC,
	PIN_VOUT0_DATA2		 = 0x00C0,
	PIN_VOUT0_DATA3		 = 0x00C4,
	PIN_VOUT0_DATA4		 = 0x00C8,
	PIN_VOUT0_DATA5		 = 0x00CC,
	PIN_VOUT0_DATA6		 = 0x00D0,
	PIN_VOUT0_DATA7		 = 0x00D4,
	PIN_VOUT0_DATA8		 = 0x00D8,
	PIN_VOUT0_DATA9		 = 0x00DC,
	PIN_VOUT0_DATA10	 = 0x00E0,
	PIN_VOUT0_DATA11	 = 0x00E4,
	PIN_VOUT0_DATA12	 = 0x00E8,
	PIN_VOUT0_DATA13	 = 0x00EC,
	PIN_VOUT0_DATA14	 = 0x00F0,
	PIN_VOUT0_DATA15	 = 0x00F4,
	PIN_GPMC0_AD8		 = 0x005C,
	PIN_GPMC0_AD9		 = 0x0060,
	PIN_GPMC0_AD10		 = 0x0064,
	PIN_GPMC0_AD11		 = 0x0068,
	PIN_GPMC0_AD12		 = 0x006C,
	PIN_GPMC0_AD13		 = 0x0070,
	PIN_GPMC0_AD14		 = 0x0074,
	PIN_GPMC0_AD15		 = 0x0078,
	PIN_GPMC0_WAIT1		 = 0x009C,
	PIN_SPI0_CS1		 = 0x01B8,
	PIN_UART0_TXD		 = 0x01CC,
	PIN_UART0_RXD		 = 0x01C8,
	PIN_SPI0_CS0		 = 0x01B4,
	PIN_MMC0_DAT3		 = 0x0208,
	PIN_I2C1_SCL		 = 0x01E8,
	PIN_I2C1_SDA		 = 0x01EC,
	PIN_MMC0_CLK		 = 0x0218,
	PIN_MMC0_CMD		 = 0x0220,
	PIN_SPI0_CLK		 = 0x01BC,
	PIN_SPI0_D0		 = 0x01C0,
	PIN_I2C0_SCL		 = 0x01E0,
	PIN_I2C0_SDA		 = 0x01E4,
	PIN_SPI0_D1		 = 0x01C4,
	PIN_MMC0_DAT0		 = 0x0214,
	PIN_MMC0_DAT5		 = 0x0200,
	PIN_MCAN0_TX		 = 0x01D8,
	PIN_MCAN0_RX		 = 0x01DC,
	PIN_MCASP0_ACLKX		 = 0x01A4,
	PIN_MCASP0_AFSX		 = 0x01A8,
	PIN_MCASP0_ACLKR		 = 0x01B0,
	PIN_MCASP0_AFSR		 = 0x01AC,
	PIN_RGMII2_RD2		 = 0x018C,
	PIN_RGMII2_RD3		 = 0x0190,
	PIN_RGMII2_TD2		 = 0x0174,
	PIN_GPMC0_DIR		 = 0x00A4,
	PIN_MCASP0_AXR3		 = 0x0194,
	PIN_MCASP0_AXR2		 = 0x0198,
	PIN_MCASP0_AXR0		 = 0x01A0,
	PIN_MCASP0_AXR1		 = 0x019C,
	PIN_GPMC0_CSN3		 = 0x00B4,
	PIN_GPMC0_WPN		 = 0x00A0,
	PIN_GPMC0_AD0		 = 0x003C,
	PIN_GPMC0_AD1		 = 0x0040,
	PIN_GPMC0_AD2		 = 0x0044,
	PIN_GPMC0_AD3		 = 0x0048,
	PIN_GPMC0_AD4		 = 0x004C,
	PIN_GPMC0_AD5		 = 0x0050,
	PIN_GPMC0_AD6		 = 0x0054,
	PIN_GPMC0_AD7		 = 0x0058,
	PIN_GPMC0_WAIT0		 = 0x0098,
	PIN_GPMC0_BE1N		 = 0x0094,
	PIN_GPMC0_CSN0		 = 0x00A8,
	PIN_GPMC0_CLK		 = 0x007C,
	PIN_GPMC0_ADVN_ALE		 = 0x0084,
	PIN_GPMC0_OEN_REN		 = 0x0088,
	PIN_GPMC0_WEN		 = 0x008C,
	PIN_GPMC0_BE0N_CLE		 = 0x0090,
	PIN_UART0_CTSN		 = 0x01D0,
	PIN_UART0_RTSN		 = 0x01D4,
	PIN_GPMC0_CSN2		 = 0x00B0,
	PIN_MMC0_DAT6		 = 0x01FC,
	PIN_MMC0_DAT7		 = 0x01F8,
	PIN_OSPI0_D6		 = 0x0024,
	PIN_OSPI0_D7		 = 0x0028,
	PIN_OSPI0_D5		 = 0x0020,
	PIN_RGMII2_TD3		 = 0x0178,
	PIN_RGMII2_TX_CTL		 = 0x0164,
	PIN_MDIO0_MDC		 = 0x0160,
	PIN_MDIO0_MDIO		 = 0x015C,
	PIN_MMC0_DAT1		 = 0x0210,
	PIN_MMC0_DAT2		 = 0x020C,
	PIN_MMC0_DAT4		 = 0x0204,
	PIN_MMC1_CMD		 = 0x023C,
	PIN_MMC1_CLK		 = 0x0234,
	PIN_MMC1_SDCD		 = 0x0240,
	PIN_MMC1_SDWP		 = 0x0244,
	PIN_MMC2_CMD		 = 0x0120,
	PIN_MMC2_CLK		 = 0x0118,
	PIN_MMC2_DAT0		 = 0x0114,
	PIN_MMC2_DAT1		 = 0x0110,
	PIN_MMC2_DAT2		 = 0x010C,
	PIN_MMC2_DAT3		 = 0x0108,
	PIN_MMC2_SDCD		 = 0x0124,
	PIN_MMC2_SDWP		 = 0x0128,
	PIN_OLDI0_A0N		 = 0x0260,
	PIN_OLDI0_A0P		 = 0x025C,
	PIN_OLDI0_A1N		 = 0x0268,
	PIN_OLDI0_A1P		 = 0x0264,
	PIN_OLDI0_A2N		 = 0x0270,
	PIN_OLDI0_A2P		 = 0x026C,
	PIN_OLDI0_A3N		 = 0x0278,
	PIN_OLDI0_A3P		 = 0x0274,
	PIN_OLDI0_A4N		 = 0x0280,
	PIN_OLDI0_A4P		 = 0x027C,
	PIN_OLDI0_A5N		 = 0x0288,
	PIN_OLDI0_A5P		 = 0x0284,
	PIN_OLDI0_A6N		 = 0x0290,
	PIN_OLDI0_A6P		 = 0x028C,
	PIN_OLDI0_A7N		 = 0x0298,
	PIN_OLDI0_A7P		 = 0x0294,
	PIN_OLDI0_CLK0N		 = 0x02A0,
	PIN_OLDI0_CLK0P		 = 0x029C,
	PIN_OLDI0_CLK1N		 = 0x02A8,
	PIN_OLDI0_CLK1P		 = 0x02A4,
	PIN_OSPI0_CLK		 = 0x0000,
	PIN_OSPI0_CSN0		 = 0x002C,
	PIN_OSPI0_CSN1		 = 0x0030,
	PIN_OSPI0_CSN2		 = 0x0034,
	PIN_OSPI0_CSN3		 = 0x0038,
	PIN_OSPI0_D0		 = 0x000C,
	PIN_OSPI0_D1		 = 0x0010,
	PIN_OSPI0_D2		 = 0x0014,
	PIN_OSPI0_D3		 = 0x0018,
	PIN_OSPI0_D4		 = 0x001C,
	PIN_OSPI0_DQS		 = 0x0008,
	PIN_RGMII1_RD0		 = 0x014C,
	PIN_RGMII1_RD1		 = 0x0150,
	PIN_RGMII1_RD2		 = 0x0154,
	PIN_RGMII1_RD3		 = 0x0158,
	PIN_RGMII1_RXC		 = 0x0148,
	PIN_RGMII1_RX_CTL		 = 0x0144,
	PIN_RGMII1_TD0		 = 0x0134,
	PIN_RGMII1_TD1		 = 0x0138,
	PIN_RGMII1_TD2		 = 0x013C,
	PIN_RGMII1_TD3		 = 0x0140,
	PIN_RGMII1_TXC		 = 0x0130,
	PIN_RGMII1_TX_CTL		 = 0x012C,
	PIN_RGMII2_RD0		 = 0x0184,
	PIN_RGMII2_RD1		 = 0x0188,
	PIN_RGMII2_RXC		 = 0x0180,
	PIN_RGMII2_RX_CTL		 = 0x017C,
	PIN_RGMII2_TD0		 = 0x016C,
	PIN_RGMII2_TD1		 = 0x0170,
	PIN_RGMII2_TXC		 = 0x0168,
	PIN_EXTINTN		 = 0x01F4,
	PIN_PORZ_OUT		 = 0x0250,
	PIN_RESETSTATZ		 = 0x024C,
	PIN_RESET_REQZ		 = 0x0248,
	PIN_GPMC0_CSN1		 = 0x00AC,
	PIN_OSPI0_LBCLKO		 = 0x0004,
	PIN_USB1_DRVVBUS		 = 0x0258,
	PIN_USB0_DRVVBUS		 = 0x0254,
    PIN_PCIE0_CLKREQN        = 0x02AC,
};

enum Pinmux_McuOffsets
{
    PIN_EMU0		 = 0x0078,
	PIN_EMU1		 = 0x007C,
	PIN_TCK		 = 0x0064,
	PIN_TDI		 = 0x006C,
	PIN_TDO		 = 0x0070,
	PIN_TMS		 = 0x0074,
	PIN_TRSTN		 = 0x0068,
	PIN_MCU_I2C0_SCL		 = 0x0044,
	PIN_MCU_I2C0_SDA		 = 0x0048,
	PIN_MCU_MCAN1_RX		 = 0x0040,
	PIN_MCU_MCAN1_TX		 = 0x003C,
	PIN_MCU_MCAN0_RX		 = 0x0038,
	PIN_MCU_MCAN0_TX		 = 0x0034,
	PIN_MCU_SPI0_CLK		 = 0x0008,
	PIN_MCU_SPI0_CS0		 = 0x0000,
	PIN_MCU_SPI0_D0		 = 0x000C,
	PIN_MCU_SPI0_D1		 = 0x0010,
	PIN_WKUP_UART0_RTSN		 = 0x0030,
	PIN_WKUP_UART0_CTSN		 = 0x002C,
	PIN_MCU_UART0_CTSN		 = 0x001C,
	PIN_MCU_UART0_RTSN		 = 0x0020,
	PIN_MCU_ERRORN		 = 0x0060,
	PIN_MCU_SPI0_CS1		 = 0x0004,
	PIN_MCU_PORZ		 = 0x0058,
	PIN_MCU_RESETSTATZ		 = 0x005C,
	PIN_MCU_RESETZ		 = 0x0054,
	PIN_MCU_UART0_RXD		 = 0x0014,
	PIN_MCU_UART0_TXD		 = 0x0018,
	PIN_WKUP_I2C0_SCL		 = 0x004C,
	PIN_WKUP_I2C0_SDA		 = 0x0050,
	PIN_WKUP_CLKOUT0		 = 0x0084,
	PIN_PMIC_LPM_EN0		 = 0x0080,
	PIN_WKUP_UART0_RXD		 = 0x0024,
	PIN_WKUP_UART0_TXD		 = 0x0028,
};
/****************************************************************************
 * Public Types
 ****************************************************************************/

struct pinmux_conf
{
    int16_t offset;
    uint32_t setting;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

void pinmux_init(void);
void pinmux_config(const struct pinmux_conf *pinmux_conf);
