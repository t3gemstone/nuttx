/****************************************************************************
 * arch/arm/src/am67/am67_lowputc.h
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
 
#ifndef __ARCH_ARM_SRC_AM67_AM67_LOWPUTC_H
#define __ARCH_ARM_SRC_AM67_AM67_LOWPUTC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
 
#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
 
#define CONSOLE_BASE	UART0_BASE
#define CONSOLE_BAUD	115200
#define CONSOLE_BITS	8
#define CONSOLE_PARITY	0
#define CONSOLE_2STOP	0

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/
 
/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: am67_lowsetup
 *
 * Description:
 *   Called at the very beginning of _start.  Performs low level
 *   initialization including setup of the console UART.  This UART done
 *   early so that the serial console is available for debugging very early
 *   in the boot sequence.
 *
 ****************************************************************************/

void am67_lowsetup(void);

#endif /* __ARCH_ARM_SRC_AM67_AM67_LOWPUTC_H */
