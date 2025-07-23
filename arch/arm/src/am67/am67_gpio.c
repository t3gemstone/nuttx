/****************************************************************************
 * arch/arm/src/am67/am67_gpio.c
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
#include <stdbool.h>
#include <errno.h>

#include <nuttx/irq.h>

#include "chip.h"
#include "arm_internal.h"
#include "am67_pinmux.h"
#include "am67_gpio.h"

/****************************************************************************
 * Name: am67_gpio_config
 ****************************************************************************/

void gpio_config(void)
{
    volatile uint32_t *gpio_reg1 = (uint32_t *)0x000F41C8;	// F19 UART0_RXD
    *gpio_reg1 = 0x00000001;
    
    
    volatile uint32_t *gpio_reg2 = (uint32_t *)0x000F41CC;	// F20 UART0_TXD
    *gpio_reg2 = 0x00000001;
}
