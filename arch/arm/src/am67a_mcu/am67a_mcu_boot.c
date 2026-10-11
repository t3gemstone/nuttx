/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_boot.c
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
#include <nuttx/init.h>

#include "arm_internal.h"
#include "am67a_mcu_mpuinit.h"
#include "am67a_mcu_lowputc.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void arm_data_initialize(void);
void am67a_mcu_board_initialize(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_boot
 *
 * Description:
 *   Complete boot operations started in arm_head.S.
 *   This is the first C function called after CPU reset.
 *
 * Boot Sequence:
 *   1. Initialize MPU regions (DDR, SRAM, TCM, MMR, and IPC buffers).
 *   2. If CONFIG_ARMV7R_MEMINIT is set, initialize .data and zero .bss.
 *   3. Setup low-level console / trace output.
 *   4. Output early greeting banner.
 *   5. Perform early board-level initialization.
 *   6. Branch to nx_start() to launch NuttX OS kernel.
 *
 ****************************************************************************/

void arm_boot(void)
{
  /* 1. Configure the Cortex-R5F MPU */

  am67a_mcu_mpu_init();

#ifdef CONFIG_ARMV7R_MEMINIT
  /* 2. Initialize .data and zero .bss after MPU is ready */

  arm_data_initialize();
#endif

  /* 3. Initialize low-level console and trace buffer */

  am67a_mcu_lowsetup();

  /* 4. Print early boot banner (mirrored to Linux trace0 and UART0) */

  am67a_mcu_lowputc_puts("\r\n========================================\r\n");
  am67a_mcu_lowputc_puts("[NuttX] T3 Gemstone O1 MCU R5F\r\n");
  am67a_mcu_lowputc_puts("[NuttX] Early Boot & MPU Initialized OK\r\n");
  am67a_mcu_lowputc_puts("========================================\r\n");

  /* 5. Early board initialization */

  am67a_mcu_board_initialize();

  /* 6. Hand over control to NuttX OS scheduler */

  nx_start();
}
