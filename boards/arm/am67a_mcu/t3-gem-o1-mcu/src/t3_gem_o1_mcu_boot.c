/****************************************************************************
 * boards/arm/am67a_mcu/t3-gem-o1-mcu/src/t3_gem_o1_mcu_boot.c
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
#include <nuttx/board.h>

#include "t3-gem-o1-mcu.h"
#include "am67a_mcu_pinmux.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_board_initialize
 *
 * Description:
 *   Perform early board-level initialization.
 *
 ****************************************************************************/

void am67a_mcu_board_initialize(void)
{
#ifdef CONFIG_16550_UART
#  ifdef CONFIG_16550_UART0
  /* UART-MAIN1: Hat Pin 10 (RX) & Pin 8 (TX) */

  am67a_mcu_uart1_pinmux_init(true);
#  endif
#  ifdef CONFIG_16550_UART1
  /* UART-MAIN6: Hat Pin 7 (RX) & Pin 11 (TX) */

  am67a_mcu_uart6_pinmux_init();
#  endif
#  ifdef CONFIG_16550_UART2
  /* UART-WKUP0: Hat Pin 26 (RX) & Pin 18 (TX) */

  am67a_mcu_wkup_uart0_pinmux_init();
#  endif
#  ifdef CONFIG_16550_UART3
  /* MCU_UART0 */

  am67a_mcu_mcu_uart0_pinmux_init();
#  endif
#endif
}

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   If CONFIG_BOARD_LATE_INITIALIZE is selected, this function is called
 *   after all sub-systems have been initialized.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  t3_gem_o1_mcu_bringup();
}
#endif
