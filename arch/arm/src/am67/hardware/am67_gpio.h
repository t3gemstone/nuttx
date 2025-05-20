/****************************************************************************
 * arch/arm/src/am67/hardware/am67_gpio.h
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

#ifndef __ARCH_ARM_SRC_AM67_HARDWARE_AM67_GPIO_H
#define __ARCH_ARM_SRC_AM67_HARDWARE_AM67_GPIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "am67_memmap.h"

/****************************************************************************
 * Pre-processor Definitions 
 ****************************************************************************/

/*
 * 9 GPIO banks each having 16 GPIO pins
 * 9*16 => 144 total of GPIO pins
*/

#define AM67_GPIO_NUM 144

/*
 * Register lengths are 32 bit, and divided into 2 banks as bank0:[0:15] and bank1:[16:31]
 * GPIOBASE + OUTDATA_OFFSET => OUTDATA01 register, := First 16 bits are for bank0 and the remaining for bank1.
 * GPIOBASE + OUTDATA_OFFSET + X * BANKOFFSET => This register is for bankX and bank(X+1)
 */

#define AM67_GPIO_BASE MCU_GPIO0

#define AM67_BANK_OFFSET          0x28 /* Difference between bank_x0 and bank_x1 */

#define AM67_GPIO_PCR_OFFSET      0x4  /* Peripheral Control Register   */
#define AM67_GPIO_BINTEN_OFFSET   0x8  /* Bit Interrupt Enable Register, [0:15] bits represents individual banks */
#define AM67_GPIO_DIR_OFFSET      0x10 /* Direction Register            */
#define AM67_GPIO_OUT_DATA_OFFSET 0x14
#define AM67_GPIO_SET_DATA_OFFSET 0x18
#define AM67_GPIO_CLR_DATA_OFFSET 0x1C
#define AM67_GPIO_IN_DATA_OFFSET  0x20

#define AM67_READ_PRIMARY_DATA    1 << 16 - 1 /* */
#define AM67_READ_SECONDARY_DATA  1 << 32 - 1 ^ AM67_READ_PRIMARY_DATA 

#endif /* __ARCH_ARM_SRC_AM67_HARDWARE_AM67_GPIO_H */
