/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_i2c.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_I2C_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_I2C_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>

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
 * Name: am67a_mcu_i2cbus_initialize
 *
 * Description:
 *   Initialize the selected I2C port:
 *     port 0: MCU_I2C0  (0x04900000) - Sensors / Header
 *     port 1: WKUP_I2C0 (0x2B200000) - On-board AT24C512C EEPROM
 *   and return a pointer to the standard NuttX I2C master device structure.
 *
 * Input Parameters:
 *   port - I2C port number (0 for MCU_I2C0, 1 for WKUP_I2C0)
 *
 * Returned Value:
 *   Valid pointer to struct i2c_master_s on success; NULL on failure.
 *
 ****************************************************************************/

FAR struct i2c_master_s *am67a_mcu_i2cbus_initialize(int port);

/****************************************************************************
 * Name: am67a_mcu_i2cbus_uninitialize
 *
 * Description:
 *   De-initialize the selected I2C port and power down the device.
 *
 * Input Parameters:
 *   dev - Device structure pointer returned by am67a_mcu_i2cbus_initialize()
 *
 * Returned Value:
 *   OK on success; a negated errno on failure.
 *
 ****************************************************************************/

int am67a_mcu_i2cbus_uninitialize(FAR struct i2c_master_s *dev);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_I2C_H */
