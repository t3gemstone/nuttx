/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_pinmux.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_PINMUX_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_PINMUX_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

#include "arm_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Pad Configuration MMR Bases (TI MCU+ SDK: CSL_MCU_PADCFG_CTRL0_CFG0_BASE) */

#define AM67A_MCU_MCU_PADCFG_BASE                 (0x04080000ul)
#define AM67A_MCU_MCU_PADCFG_SIZE                 (0x00008000ul)

#define AM67A_MCU_PADCFG_PMUX_OFFSET              (0x00004000u)

/* MMR Lock / Kick Offsets */

#define AM67A_MCU_MCU_PADCONFIG_LOCK0_KICK0       (0x1008u)
#define AM67A_MCU_MCU_PADCONFIG_LOCK0_KICK1       (0x100cu)
#define AM67A_MCU_MCU_PADCONFIG_LOCK1_KICK0       (0x5008u)
#define AM67A_MCU_MCU_PADCONFIG_LOCK1_KICK1       (0x500cu)

#define AM67A_MCU_KICK0_UNLOCK_VAL                (0x68ef3490u)
#define AM67A_MCU_KICK1_UNLOCK_VAL                (0xd172bc5au)

/* Pad Configuration Bitfields */

#define AM67A_MCU_PIN_MODE(mode)                  ((uint32_t)(mode) & 0x0fu)
#define AM67A_MCU_PIN_PULL_DISABLE                ((uint32_t)1u << 16u)
#define AM67A_MCU_PIN_PULL_DIRECTION             ((uint32_t)1u << 17u)
#define AM67A_MCU_PIN_INPUT_ENABLE                ((uint32_t)1u << 18u)
#define AM67A_MCU_PIN_OUTPUT_DISABLE              ((uint32_t)1u << 21u)
#define AM67A_MCU_PIN_WAKEUP_ENABLE               ((uint32_t)1u << 29u)

/* MCU Pad Offsets (relative to AM67A_MCU_MCU_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET) */

#define AM67A_MCU_PIN_MCU_SPI0_CS0                (0x0000u)
#define AM67A_MCU_PIN_MCU_SPI0_CS1                (0x0004u)
#define AM67A_MCU_PIN_MCU_SPI0_CLK                (0x0008u)
#define AM67A_MCU_PIN_MCU_SPI0_D0                 (0x000cu)
#define AM67A_MCU_PIN_MCU_SPI0_D1                 (0x0010u)
#define AM67A_MCU_PIN_MCU_UART0_RXD               (0x0014u)
#define AM67A_MCU_PIN_MCU_UART0_TXD               (0x0018u)
#define AM67A_MCU_PIN_WKUP_UART0_RXD              (0x0024u)
#define AM67A_MCU_PIN_WKUP_UART0_TXD              (0x0028u)
#define AM67A_MCU_PIN_WKUP_UART0_RTSN             (0x0030u)
#define AM67A_MCU_PIN_MCU_MCAN0_TX                (0x0034u)
#define AM67A_MCU_PIN_MCU_I2C0_SCL                (0x0044u)
#define AM67A_MCU_PIN_MCU_I2C0_SDA                (0x0048u)
#define AM67A_MCU_PIN_WKUP_I2C0_SCL               (0x004cu)
#define AM67A_MCU_PIN_WKUP_I2C0_SDA               (0x0050u)

/* Main Domain Pad Configuration MMR Base (TI MCU+ SDK: CSL_PADCFG_CTRL0_CFG0_BASE) */

#define AM67A_MCU_MAIN_PADCFG_BASE                (0x000f0000ul)
#define AM67A_MCU_MAIN_CTRL_MMR_BASE              (0x00100000ul)

#define AM67A_MCU_MAIN_LOCK1_KICK0                (0x00005008u)
#define AM67A_MCU_MAIN_LOCK1_KICK1                (0x0000500cu)
#define AM67A_MCU_MAIN_EPWM_TB_CLKEN              (0x00004130u)

/* Main Pad Offsets (relative to AM67A_MCU_MAIN_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET) */

#define AM67A_MCU_PIN_EPWM0_A                     (0x01b4u) /* SPI0_CS0    -> Mode 2 EHRPWM0_A (Hat Pin 29) */
#define AM67A_MCU_PIN_EPWM0_B                     (0x01b0u) /* MCASP0_ACLKR -> Mode 6 EHRPWM0_B (Hat Pin 8)  */
#define AM67A_MCU_PIN_EPWM1_A                     (0x01bcu) /* SPI0_CLK    -> Mode 2 EHRPWM1_A (Hat Pin 31) */
#define AM67A_MCU_PIN_EPWM1_B                     (0x01c0u) /* SPI0_D0     -> Mode 2 EHRPWM1_B (Hat Pin 33) */
#define AM67A_MCU_PIN_ECAP0                       (0x01b8u) /* SPI0_CS1    -> Mode 3 ECAP0_IN_APWM_OUT (Hat Pin 32) */
#define AM67A_MCU_PIN_ECAP1                       (0x019cu) /* MCASP0_AXR1 -> Mode 2 ECAP1_IN_APWM_OUT (Hat Pin 36) */
#define AM67A_MCU_PIN_ECAP2                       (0x01a4u) /* MCASP0_ACLKX -> Mode 2 ECAP2_IN_APWM_OUT (Hat Pin 12) */
#define AM67A_MCU_PIN_UART1_RXD                   (0x01acu) /* MCASP0_AFSR  -> Mode 2 UART1_RXD (Hat Pin 10) */
#define AM67A_MCU_PIN_UART1_TXD                   (0x01b0u) /* MCASP0_ACLKR -> Mode 2 UART1_TXD (Hat Pin 8)  */
#define AM67A_MCU_PIN_UART0_RXD                   (0x01c8u) /* UART0_RXD    -> Mode 0 UART0_RXD (Serial Hdr) */
#define AM67A_MCU_PIN_UART0_TXD                   (0x01ccu) /* UART0_TXD    -> Mode 0 UART0_TXD (Serial Hdr) */
#define AM67A_MCU_PIN_UART6_RXD                   (0x009cu) /* GPMC0_WAIT1  -> Mode 3 UART6_RXD (Hat Pin 7) */
#define AM67A_MCU_PIN_UART6_TXD                   (0x0198u) /* MCASP0_AXR2  -> Mode 3 UART6_TXD (Hat Pin 11) */

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_mcu_pinmux_unlock
 *
 * Description:
 *   Unlock MCU Domain pad configuration MMR partition.
 *
 ****************************************************************************/

static inline void am67a_mcu_mcu_pinmux_unlock(void)
{
  uint32_t base = AM67A_MCU_MCU_PADCFG_BASE;

  /* Lock 0 */

  putreg32(AM67A_MCU_KICK0_UNLOCK_VAL, base + AM67A_MCU_MCU_PADCONFIG_LOCK0_KICK0);
  putreg32(AM67A_MCU_KICK1_UNLOCK_VAL, base + AM67A_MCU_MCU_PADCONFIG_LOCK0_KICK1);

  /* Lock 1 */

  putreg32(AM67A_MCU_KICK0_UNLOCK_VAL, base + AM67A_MCU_MCU_PADCONFIG_LOCK1_KICK0);
  putreg32(AM67A_MCU_KICK1_UNLOCK_VAL, base + AM67A_MCU_MCU_PADCONFIG_LOCK1_KICK1);
}

/****************************************************************************
 * Name: am67a_mcu_main_pinmux_unlock
 *
 * Description:
 *   Unlock Main Domain pad configuration MMR partition 1.
 *
 ****************************************************************************/

static inline void am67a_mcu_main_pinmux_unlock(void)
{
  uint32_t base = AM67A_MCU_MAIN_PADCFG_BASE;

  /* Lock 0 */

  putreg32(AM67A_MCU_KICK0_UNLOCK_VAL, base + AM67A_MCU_MCU_PADCONFIG_LOCK0_KICK0);
  putreg32(AM67A_MCU_KICK1_UNLOCK_VAL, base + AM67A_MCU_MCU_PADCONFIG_LOCK0_KICK1);

  /* Lock 1 */

  putreg32(AM67A_MCU_KICK0_UNLOCK_VAL, base + AM67A_MCU_MAIN_LOCK1_KICK0);
  putreg32(AM67A_MCU_KICK1_UNLOCK_VAL, base + AM67A_MCU_MAIN_LOCK1_KICK1);
}

/****************************************************************************
 * Name: am67a_mcu_main_ctrl_mmr_unlock
 *
 * Description:
 *   Unlock Main Domain Control MMR partition 1.
 *
 ****************************************************************************/

static inline void am67a_mcu_main_ctrl_mmr_unlock(void)
{
  uint32_t base = AM67A_MCU_MAIN_CTRL_MMR_BASE;

  putreg32(AM67A_MCU_KICK0_UNLOCK_VAL, base + AM67A_MCU_MAIN_LOCK1_KICK0);
  putreg32(AM67A_MCU_KICK1_UNLOCK_VAL, base + AM67A_MCU_MAIN_LOCK1_KICK1);
}

/****************************************************************************
 * Name: am67a_mcu_pwm_tbclk_enable
 *
 * Description:
 *   Enable Time-Base Clock (TBCLK) for EPWM0 and EPWM1 modules in Main CTRL MMR.
 *
 ****************************************************************************/

static inline void am67a_mcu_pwm_tbclk_enable(void)
{
  am67a_mcu_main_ctrl_mmr_unlock();

  /* Set bit 0 (EPWM0_TB_CLKEN) and bit 1 (EPWM1_TB_CLKEN) */

  uint32_t reg_addr = AM67A_MCU_MAIN_CTRL_MMR_BASE + AM67A_MCU_MAIN_EPWM_TB_CLKEN;
  uint32_t val = getreg32(reg_addr);
  putreg32(val | 0x03u, reg_addr);
}

/****************************************************************************
 * Name: am67a_mcu_pwm_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for the 4 motor PWM outputs on expansion header:
 *     CH0 (Motor 1): Hat Pin 29 -> EPWM0_A (Pad 0x1b4, Mode 2)
 *     CH1 (Motor 2): Hat Pin 8  -> EPWM0_B (Pad 0x1b0, Mode 6)
 *     CH2 (Motor 3): Hat Pin 31 -> EPWM1_A (Pad 0x1bc, Mode 2)
 *     CH3 (Motor 4): Hat Pin 33 -> EPWM1_B (Pad 0x1c0, Mode 2)
 *
 ****************************************************************************/

static inline void am67a_mcu_pwm_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MAIN_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_main_pinmux_unlock();

  /* CH0: Hat Pin 29 -> SPI0_CS0 pad as EHRPWM0_A (Mode 2) */

  putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_EPWM0_A);

#ifndef CONFIG_16550_UART0
  /* CH1: Hat Pin 8 -> MCASP0_ACLKR pad as EHRPWM0_B (Mode 6)
   * Preserved for UART1_TXD (Mode 3) when CONFIG_16550_UART0 is enabled.
   */

  putreg32(AM67A_MCU_PIN_MODE(6) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_EPWM0_B);
#endif

  /* CH2: Hat Pin 31 -> SPI0_CLK pad as EHRPWM1_A (Mode 2) */

  putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_EPWM1_A);

  /* CH3: Hat Pin 33 -> SPI0_D0 pad as EHRPWM1_B (Mode 2) */

  putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_EPWM1_B);

  /* CH4: Hat Pin 32 -> SPI0_CS1 pad as ECAP0_IN_APWM_OUT (Mode 3) */

  putreg32(AM67A_MCU_PIN_MODE(3) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_ECAP0);

  /* CH5: Hat Pin 36 -> MCASP0_AXR1 pad as ECAP1_IN_APWM_OUT (Mode 2) */

  putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_ECAP1);

  /* CH6: Hat Pin 12 -> MCASP0_ACLKX pad as ECAP2_IN_APWM_OUT (Mode 2) */

  putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_ECAP2);
}

/****************************************************************************
 * Name: am67a_mcu_spi0_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for MCU_SPI0 peripheral (CLK, CS0, CS1, CS3, D0, D1, IMU_EN).
 *
 ****************************************************************************/

static inline void am67a_mcu_spi0_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MCU_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_mcu_pinmux_unlock();

  /* MCU_SPI0_CLK: Mode 0, Input buffer enabled, Pull-Up enabled (for Mode 3) */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_SPI0_CLK);

  /* MCU_SPI0_CS0: Mode 0, Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_SPI0_CS0);

  /* MCU_SPI0_CS1 (BMP390, CS1): Mode 0, Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_SPI0_CS1);

  /* MCU_SPI0_CS3 (ICM20948, CS3): MCU_MCAN0_TX pad, Mode 2, Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_MCAN0_TX);

  /* MCU_SPI0_D0 (MOSI): Mode 0, Input buffer enabled, Pull-Down enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE,
           pmux_base + AM67A_MCU_PIN_MCU_SPI0_D0);

  /* MCU_SPI0_D1 (MISO): Mode 0, Input buffer enabled, Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_SPI0_D1);
}

/****************************************************************************
 * Name: am67a_mcu_imu_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for IMU_EN pin (MCU_GPIO0_12 on WKUP_UART0_RTSN).
 *
 ****************************************************************************/

static inline void am67a_mcu_imu_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MCU_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_mcu_pinmux_unlock();

  /* IMU_EN -> MCU_GPIO0_12 (WKUP_UART0_RTSN pad, Mode 7, Pull-Down enabled) */

  putreg32(AM67A_MCU_PIN_MODE(7) | AM67A_MCU_PIN_INPUT_ENABLE,
           pmux_base + AM67A_MCU_PIN_WKUP_UART0_RTSN);
}

/****************************************************************************
 * Name: am67a_mcu_i2c0_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for MCU_I2C0 peripheral (SCL, SDA).
 *
 ****************************************************************************/

static inline void am67a_mcu_i2c0_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MCU_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_mcu_pinmux_unlock();

  /* MCU_I2C0_SCL: Mode 0, Input buffer enabled, Internal Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_I2C0_SCL);

  /* MCU_I2C0_SDA: Mode 0, Input buffer enabled, Internal Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_I2C0_SDA);
}

/****************************************************************************
 * Name: am67a_mcu_wkup_i2c0_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for WKUP_I2C0 peripheral (SCL, SDA)
 *   connecting to the on-board AT24C512C EEPROM.
 *
 ****************************************************************************/

static inline void am67a_mcu_wkup_i2c0_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MCU_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_mcu_pinmux_unlock();

  /* WKUP_I2C0_SCL: Mode 0, Input buffer enabled, Internal Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_WKUP_I2C0_SCL);

  /* WKUP_I2C0_SDA: Mode 0, Input buffer enabled, Internal Pull-Up enabled */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_WKUP_I2C0_SDA);
}

/****************************************************************************
 * Name: am67a_mcu_uart1_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for UART-MAIN1 (Hat Pin 8 TX, Hat Pin 10 RX).
 *   If enable_tx is false, Hat Pin 8 is preserved for PWM (EPWM0_B).
 *
 ****************************************************************************/

static inline void am67a_mcu_uart1_pinmux_init(bool enable_tx)
{
  uint32_t pmux_base = AM67A_MCU_MAIN_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_main_pinmux_unlock();

  /* Hat Pin 10 -> MCASP0_AFSR pad as UART1_RXD (Mode 2, Input Enable, Pull-Up) */

  putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_UART1_RXD);

  if (enable_tx)
    {
      /* Hat Pin 8 -> MCASP0_ACLKR pad as UART1_TXD (Mode 2, Pull Disable) */

      putreg32(AM67A_MCU_PIN_MODE(2) | AM67A_MCU_PIN_PULL_DISABLE,
               pmux_base + AM67A_MCU_PIN_UART1_TXD);
    }
}

/****************************************************************************
 * Name: am67a_mcu_uart6_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for UART-MAIN6 (Hat Pin 11 TX, Hat Pin 7 RX).
 *
 ****************************************************************************/

static inline void am67a_mcu_uart6_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MAIN_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_main_pinmux_unlock();

  /* Hat Pin 7 -> GPMC0_WAIT1 pad as UART6_RXD (Mode 3, Input Enable, Pull-Up) */

  putreg32(AM67A_MCU_PIN_MODE(3) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_UART6_RXD);

  /* Hat Pin 11 -> MCASP0_AXR2 pad as UART6_TXD (Mode 3, Pull Disable) */

  putreg32(AM67A_MCU_PIN_MODE(3) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_UART6_TXD);
}

/****************************************************************************
 * Name: am67a_mcu_wkup_uart0_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for UART-WKUP0 (Hat Pin 18 TX, Hat Pin 26 RX).
 *
 ****************************************************************************/

static inline void am67a_mcu_wkup_uart0_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MCU_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_mcu_pinmux_unlock();

  /* Hat Pin 26 -> WKUP_UART0_RXD (Pad 0x0024, Mode 0, Input Enable, Pull-Up) */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_WKUP_UART0_RXD);

  /* Hat Pin 18 -> WKUP_UART0_TXD (Pad 0x0028, Mode 0, Pull Disable) */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_WKUP_UART0_TXD);
}

/****************************************************************************
 * Name: am67a_mcu_mcu_uart0_pinmux_init
 *
 * Description:
 *   Configure pin multiplexing for MCU_UART0.
 *
 ****************************************************************************/

static inline void am67a_mcu_mcu_uart0_pinmux_init(void)
{
  uint32_t pmux_base = AM67A_MCU_MCU_PADCFG_BASE + AM67A_MCU_PADCFG_PMUX_OFFSET;

  am67a_mcu_mcu_pinmux_unlock();

  /* MCU_UART0_RXD (Pad 0x0014, Mode 0, Input Enable, Pull-Up) */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_INPUT_ENABLE |
           AM67A_MCU_PIN_PULL_DIRECTION,
           pmux_base + AM67A_MCU_PIN_MCU_UART0_RXD);

  /* MCU_UART0_TXD (Pad 0x0018, Mode 0, Pull Disable) */

  putreg32(AM67A_MCU_PIN_MODE(0) | AM67A_MCU_PIN_PULL_DISABLE,
           pmux_base + AM67A_MCU_PIN_MCU_UART0_TXD);
}

#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_PINMUX_H */
