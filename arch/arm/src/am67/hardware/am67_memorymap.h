/****************************************************************************
 * arch/arm/src/am67/hardware/am67_memorymap.h
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

#ifndef __ARCH_ARM_SRC_AM67_HARDWARE_AM67_MEMORYMAP_H
#define __ARCH_ARM_SRC_AM67_HARDWARE_AM67_MEMORYMAP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_ARCH_CHIP_AM67)
#  include "hardware/am67_memorymap.h"
#else
#  error Unrecognized AM67 chip
#endif

#define UART0_BASE			0x0002800000
#define UART0_SIZE			0x200 // 512 B

#define MCU_PLLCTRL0_BASE		0x0004020000
#define MCU_PLLCTRL0_SIZE		0x200 // 512 B

#define MCU_GPIO0_BASE			0x0004201000
#define MCU_GPIO0_SIZE			0x100 // 256 B

#define MCU_TIMER0_CFG_BASE		0x0004800000
#define MCU_TIMER0_CFG_SIZE		0x400 // 1 KB

#define MCU_TIMER1_CFG_BASE		0x0004810000
#define MCU_TIMER1_CFG_SIZE		0x400 // 1 KB

#define MCU_TIMER2_CFG_BASE		0x0004820000
#define MCU_TIMER2_CFG_SIZE		0x400 // 1 KB

#define MCU_TIMER3_CFG_BASE		0x0004830000
#define MCU_TIMER3_CFG_SIZE		0x400 // 1 KB

#define MCU_UART0_BASE			0x0004A00000
#define MCU_UART0_SIZE			0x200 // 512 B

#define MCU_R5FSS0_CORE0_ICACHE_BASE	0x0073000000
#define MCU_R5FSS0_CORE0_ICACHE_SIZE	0x00800000 // 8 MB

#define MCU_R5FSS0_CORE0_DCACHE_BASE	0x0073800000
#define MCU_R5FSS0_CORE0_DCACHE_SIZE	0x00800000 // 8 MB

#define MCU_R5FSS0_CORE0_ATCM_BASE	0x0079000000
#define MCU_R5FSS0_CORE0_ATCM_SIZE	0x800 // 8 KB

#define MCU_R5FSS0_CORE0_BTCM_BASE	0x0079020000
#define MCU_R5FSS0_CORE0_BTCM_SIZE	0x800 // 8 KB

#define MCU_MSRAM_256K0_RAM_BASE	0x0079100000
#define MCU_MSRAM_256K0_RAM_SIZE	0x40000 // 256 KB

#define MCU_MSRAM_256K1_RAM_BASE	0x0079140000
#define MCU_MSRAM_256K1_RAM_SIZE	0x40000 // 256 KB

#endif /* __ARCH_ARM_SRC_AM67_HARDWARE_AM67_MEMORYMAP_H */

