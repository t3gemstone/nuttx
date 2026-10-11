/****************************************************************************
 * boards/arm/am67a_mcu/t3-gem-o1-mcu/src/t3_gem_o1_mcu_bringup.c
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
#include <nuttx/fs/fs.h>
#include <debug.h>

#include "t3-gem-o1-mcu.h"

#ifdef CONFIG_AM67A_MCU_RPTUN
#  include "am67a_mcu_rptun.h"
#endif

#ifdef CONFIG_RPMSG_UART_RAW
#  include <nuttx/serial/uart_rpmsg_raw.h>
#endif

#if defined(CONFIG_SPI_DRIVER) && (defined(CONFIG_AM67A_MCU_MCSPI0) || defined(CONFIG_AM67A_MCU_MCSPI1))
#  include <nuttx/spi/spi_transfer.h>
#  include "am67a_mcu_mcspi.h"
#  include "am67a_mcu_pinmux.h"
#  include "arm_internal.h"
#endif

#if defined(CONFIG_I2C_DRIVER) && defined(CONFIG_AM67A_MCU_I2C0)
#  include <nuttx/i2c/i2c_master.h>
#  include "am67a_mcu_i2c.h"
#  if defined(CONFIG_I2C_EE_24XX)
#    include <nuttx/eeprom/i2c_xx24xx.h>
#  endif
#  if defined(CONFIG_MTD_AT24XX)
#    include <nuttx/mtd/mtd.h>
#    include <nuttx/fs/fs.h>
#  endif
#endif

#include <arch/chip/am67a_mcu_hrt.h>
#include <arch/chip/am67a_mcu_pwm.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_RPMSG_UART_RAW
/****************************************************************************
 * Name: rpmsg_serialrawinit
 *
 * Description:
 *   Initialize RPMsg raw UART driver for console communication.
 *
 ****************************************************************************/

void rpmsg_serialrawinit(void)
{
  uart_rpmsg_raw_init("a53", "RPMSG0", 512, true);
  uart_rpmsg_raw_init("a53", "RPMSG1", 1024, false);
}
#endif

/****************************************************************************
 * Name: t3_gem_o1_mcu_imu_power
 *
 * Description:
 *   Control power / regulator enable for ICM-20948 IMU (MCU_GPIO0_12).
 *   Active-LOW (true: Powered, false: Power-Down).
 *
 ****************************************************************************/

void t3_gem_o1_mcu_imu_power(bool enable)
{
  uint32_t pin_mask = (1u << 12);

  /* Configure pad to Mode 7 (MCU_GPIO0_12) */

  am67a_mcu_imu_pinmux_init();

  /* Set direction to output (MCU_GPIO0 DIR register at 0x04201010) */

  putreg32(getreg32(0x04201010ul) & ~pin_mask, 0x04201010ul);

  if (enable)
    {
      /* Drive LOW (active) to power on IMU (CLR register at 0x0420101c) */

      putreg32(pin_mask, 0x0420101cul);
    }
  else
    {
      /* Drive HIGH to power down IMU (SET register at 0x04201018) */

      putreg32(pin_mask, 0x04201018ul);
    }
}

/****************************************************************************
 * Name: t3_gem_o1_mcu_bringup
 *
 * Description:
 *   Perform architecture-specific board bringup.
 *
 ****************************************************************************/

int t3_gem_o1_mcu_bringup(void)
{
  static bool g_bringup_done = false;
  int ret = OK;

  if (g_bringup_done)
    {
      return OK;
    }

  g_bringup_done = true;

  /* Initialize High Resolution Timer subsystem (MCU_TIMER1) */

  hrt_init();

#ifdef CONFIG_FS_PROCFS
  /* Mount procfs */

  ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      serr("ERROR: Failed to mount procfs at /proc: %d\n", ret);
    }
#endif

#ifdef CONFIG_AM67A_MCU_RPTUN
  /* Initialize AM67A MCU RemoteProc Tunnel for IPC with Linux */

  ret = am67a_mcu_rptun_init("a53");
  if (ret < 0)
    {
      serr("ERROR: Failed to initialize AM67A MCU RPTun: %d\n", ret);
    }
#endif

#ifdef CONFIG_NET
  /* Initialize network bridge (tap0 over RPMSG1) */

  ret = t3_gem_o1_mcu_netinit();
  if (ret < 0)
    {
      serr("ERROR: Failed to initialize network bridge: %d\n", ret);
    }
#endif

#if defined(CONFIG_SPI_DRIVER) && defined(CONFIG_AM67A_MCU_MCSPI0)
  /* Power on onboard ICM-20948 IMU */

  t3_gem_o1_mcu_imu_power(true);

  /* Register /dev/spi0 driver node for spitool / user apps */

  FAR struct spi_dev_s *spi0 = am67a_mcu_spibus_initialize(0);
  if (spi0 != NULL)
    {
      ret = spi_register(spi0, 0);
      if (ret < 0)
        {
          serr("ERROR: Failed to register /dev/spi0: %d\n", ret);
        }
    }
#endif

#if defined(CONFIG_I2C_DRIVER) && defined(CONFIG_AM67A_MCU_I2C0)
  /* Register /dev/i2c0 driver node for i2ctool / user apps */

  FAR struct i2c_master_s *i2c0 = am67a_mcu_i2cbus_initialize(0);
  if (i2c0 != NULL)
    {
      ret = i2c_register(i2c0, 0);
      if (ret < 0)
        {
          serr("ERROR: Failed to register /dev/i2c0: %d\n", ret);
        }
    }

  /* Register /dev/i2c1 (WKUP_I2C0) and on-board AT24C512C EEPROM */

  FAR struct i2c_master_s *i2c1 = am67a_mcu_i2cbus_initialize(1);
  if (i2c1 != NULL)
    {
      ret = i2c_register(i2c1, 1);
      if (ret < 0)
        {
          serr("ERROR: Failed to register /dev/i2c1: %d\n", ret);
        }

#if defined(CONFIG_I2C_EE_24XX)
      /* Bind AT24C512C (64 KiB) EEPROM at address 0x51 to /dev/eeprom */

      ret = ee24xx_initialize(i2c1, 0x51, "/dev/eeprom",
                              EEPROM_AT24C512, false);
      if (ret < 0)
        {
          serr("ERROR: Failed to initialize /dev/eeprom: %d\n", ret);
        }
#endif

#if defined(CONFIG_MTD_AT24XX)
      /* Bind AT24C512C as an MTD device for parameter storage */

      FAR struct mtd_dev_s *mtd = at24c_initialize(i2c1);
      if (mtd != NULL)
        {
          ret = register_mtddriver("/fs/mtd_params", mtd, 0755, NULL);
          if (ret < 0)
            {
              serr("ERROR: Failed to register /fs/mtd_params: %d\n", ret);
            }
        }
#endif
    }
#endif

  /* Initialize ePWM subsystem (Pinmux, Clocks, EPWM0, EPWM1) */

  ret = am67a_mcu_pwm_init();
  if (ret < 0)
    {
      serr("ERROR: Failed to initialize ePWM: %d\n", ret);
    }

#ifdef CONFIG_PWM
  FAR struct pwm_lowerhalf_s *pwm;
  char devname[16];
  int t;

  for (t = 0; t < 5; t++)
    {
      pwm = am67a_mcu_pwminit(t);
      if (pwm != NULL)
        {
          snprintf(devname, sizeof(devname), "/dev/pwm%d", t);
          pwm_register(devname, pwm);
        }
    }
#endif /* CONFIG_PWM */

#ifdef CONFIG_16550_UART
#  ifdef CONFIG_16550_UART0
  /* Re-assert UART1 pinmux on Hat Pin 8 (TX) and Pin 10 (RX) */

  am67a_mcu_uart1_pinmux_init(true);
#  endif
#  ifdef CONFIG_16550_UART1
  /* Re-assert UART6 pinmux on Hat Pin 11 (TX) and Pin 7 (RX) */

  am67a_mcu_uart6_pinmux_init();
#  endif
#  ifdef CONFIG_16550_UART2
  /* Re-assert WKUP_UART0 pinmux on Hat Pin 18 (TX) and Pin 26 (RX) */

  am67a_mcu_wkup_uart0_pinmux_init();
#  endif
#  ifdef CONFIG_16550_UART3
  /* Re-assert MCU_UART0 pinmux */

  am67a_mcu_mcu_uart0_pinmux_init();
#  endif
#endif

  return ret;
}
