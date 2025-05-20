/****************************************************************************
 * arch/arm/src/am67/am67_boot.c
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

#include <stdint.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/cache.h>
#include <nuttx/init.h>

#include <arch/board/board.h>

#include "chip.h"
#include "arm.h"
#include "arm_internal.h"
#include "am67_boot.h"

#include "am67_rsc.h"
#include "am67_gpio.h"
#include "pinmux.h"
#include "addr_translate.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/


void am67_board_initialize(void)
{
}


void am67_memory_initialize(void)
{

}

void Reset_Handler(void) {
    // Perform reset-related initialization and jump to main()
}

void NMI_Handler(void) {
    // Handle the NMI exception
}

void HardFault_Handler(void) {
    // Handle the hard fault exception
}

uint32_t _estack;

__attribute__((section(".vectors"))) const void *g_pfnVectors[] = {
    (void *)&_estack,        // The initial stack pointer
    (void *)Reset_Handler,   // The reset handler
    (void *)NMI_Handler,     // The NMI handler
    (void *)HardFault_Handler, // The hard fault handler
    // Add other exception and interrupt handlers here
};

/****************************************************************************
 * Name: arm_boot
 *
 * Description:
 *   Complete boot operations started in arm_head.S
 *
 *   This logic will be executing in SDRAM.  This boot logic was started by
 *   the AM67 boot logic.  At this point in time, clocking and SDRAM have
 *   already be initialized (they must be because we are executing out of
 *   SDRAM).  So all that must be done here is to:
 *
 *     1) Refine the memory mapping,
 *     2) Configure the serial console, and
 *     3) Perform board-specific initializations.
 *
 ****************************************************************************/



void arm_boot(void)
{
  /* Initialize the FPU */

  
  arm_fpuconfig();

  /* Disable CPU Watchdog */

  am67_memory_initialize();

#ifdef USE_EARLYSERIALINIT
  /* Perform early serial initialization if we are going to use the serial
   * driver.
   */

  //arm_earlyserialinit();
#endif

  /* Perform board-specific initialization,  This must include:
   *
   * - Initialization of board-specific memory resources (e.g., SDRAM)
   * - Configuration of board specific resources (PIOs, LEDs, etc).
   */

  am67_board_initialize();
  //blink_internal_led_demo();  
  nx_start();
}