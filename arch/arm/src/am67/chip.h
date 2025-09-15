/****************************************************************************
 * arch/arm/src/am67/chip.h
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

#ifndef __ARCH_ARM_SRC_AM67_CHIP_H
#define __ARCH_ARM_SRC_AM67_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include "hardware/am67_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CSL_REG32_WR(p, v)      (CSL_REG32_WR_RAW((volatile uint32_t *)(p), (uint32_t)(v)))
#define CSL_REG32_RD(p)         (CSL_REG32_RD_RAW((volatile uint32_t *) (p)))


#define CHIP_MPCORE_VBASE	 (0x0001800000)
                                    
static inline void CSL_REG32_WR_RAW(volatile uint32_t * const p, uint32_t v);
static inline uint32_t CSL_REG32_RD_RAW(volatile const uint32_t * const p);

/****************************************************************************
 * Name: CSL_REG32_WR_RAW
 * Description
 * writes a 32-bit value directly to a given memory-mapped register address
 ****************************************************************************/
static inline void CSL_REG32_WR_RAW(volatile uint32_t * const p, uint32_t v)
{
    *p = v;
    return;
}

/****************************************************************************
 * Name: CSL_REG32_RD_RAW
 * reads and returns a 32-bit value from a specified register address
 ****************************************************************************/
static inline uint32_t CSL_REG32_RD_RAW(volatile const uint32_t * const p)
{
    return (*p);
}


#endif /* __ARCH_ARM_SRC_AM67_CHIP_H */
