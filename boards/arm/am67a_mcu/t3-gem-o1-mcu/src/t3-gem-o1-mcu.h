/****************************************************************************
 * boards/arm/am67a_mcu/t3-gem-o1-mcu/src/t3-gem-o1-mcu.h
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

#ifndef __BOARDS_ARM_AM67_T3_GEM_O1_MCU_SRC_T3_GEM_O1_MCU_H
#define __BOARDS_ARM_AM67_T3_GEM_O1_MCU_SRC_T3_GEM_O1_MCU_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: t3_gem_o1_mcu_bringup
 *
 * Description:
 *   Bring up board-level devices and mount filesystems.
 *
 ****************************************************************************/

int t3_gem_o1_mcu_bringup(void);

/****************************************************************************
 * Name: t3_gem_o1_mcu_imu_power
 *
 * Description:
 *   Control power / regulator enable for ICM-20948 IMU (MCU_GPIO0_12).
 *   Active-LOW (true: Powered, false: Power-Down).
 *
 ****************************************************************************/

void t3_gem_o1_mcu_imu_power(bool enable);

#ifdef CONFIG_NET
/****************************************************************************
 * Name: t3_gem_o1_mcu_netinit
 *
 * Description:
 *   Initialize network TAP bridge over RPMsg.
 *
 ****************************************************************************/

int t3_gem_o1_mcu_netinit(void);
#endif

#endif /* __BOARDS_ARM_AM67_T3_GEM_O1_MCU_SRC_T3_GEM_O1_MCU_H */
