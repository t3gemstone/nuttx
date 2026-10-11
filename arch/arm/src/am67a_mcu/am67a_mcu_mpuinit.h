/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_mpuinit.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_MPUINIT_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_MPUINIT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <sys/types.h>

#include "mpu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Cortex-R5F supports up to 16 MPU regions */

#define AM67A_MCU_NUM_OF_MPU_REGIONS          (16)

/* Region base addresses */

#define AM67A_MCU_REGISTER_START_ADDR         (0x00000000ul)
#define AM67A_MCU_TCMA_START_ADDR             (0x00000000ul)
#define AM67A_MCU_TCMB_START_ADDR             (0x41010000ul)
#define AM67A_MCU_FLASH_START_ADDR            (0x60000000ul)
#define AM67A_MCU_MCU_MSRAM_START_ADDR        (0x79100000ul)
#define AM67A_MCU_DDR_START_ADDR              (0x80000000ul)
#define AM67A_MCU_LINUX_VRING_START_ADDR      (0xa1000000ul)
#define AM67A_MCU_LINUX_RSCTBL_START_ADDR     (0xa1100000ul)

/* Region sizes */

#define AM67A_MCU_REGISTER_SIZE               (2ul * 1024 * 1024 * 1024)
#define AM67A_MCU_TCMA_SIZE                   (32ul * 1024)
#define AM67A_MCU_TCMB_SIZE                   (32ul * 1024)
#define AM67A_MCU_FLASH_SIZE                  (64ul * 1024 * 1024)
#define AM67A_MCU_MCU_MSRAM_SIZE              (512ul * 1024)
#define AM67A_MCU_DDR_SIZE                    (2ul * 1024 * 1024 * 1024)
#define AM67A_MCU_LINUX_VRING_SIZE            (1ul * 1024 * 1024)
#define AM67A_MCU_LINUX_RSCTBL_SIZE           (1ul * 1024 * 1024)

/* Background region bit in SCTLR */

#define AM67A_MCU_SCTLR_BR                    (1 << 17)

/* Region attributes:
 *
 * Region 0: Registers / MMRs
 *   Device, Strongly-Ordered, Shareable, Execute Never, Full RW
 */

#define am67a_mcu_register_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_S       | \
                                            MPU_RACR_B       | \
                                            MPU_RACR_AP_RWRW | \
                                            MPU_RACR_XN)

/* Region 1 & 2: TCMA and TCMB
 *   Normal, Non-cached, Bufferable, Execute Permitted, Full RW
 */

#define am67a_mcu_tcma_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_TEX(1) | \
                                            MPU_RACR_B      | \
                                            MPU_RACR_AP_RWRW)

#define am67a_mcu_tcmb_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_TEX(1) | \
                                            MPU_RACR_B      | \
                                            MPU_RACR_AP_RWRW)

/* Region 3: MCU MSRAM
 *   Normal, Cacheable WB/WA, Bufferable, Execute Permitted, Full RW
 */

#define am67a_mcu_mcu_msram_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_TEX(1) | \
                                            MPU_RACR_C      | \
                                            MPU_RACR_B      | \
                                            MPU_RACR_AP_RWRW)

/* Region 4: Flash (OSPI)
 *   Normal, Cacheable WB, Bufferable, Execute Permitted, Full RW
 */

#define am67a_mcu_flash_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_TEX(1) | \
                                            MPU_RACR_C      | \
                                            MPU_RACR_B      | \
                                            MPU_RACR_AP_RWRW)

/* Region 5: DDR RAM
 *   Normal, Cacheable WB/WA, Bufferable, Execute Permitted, Full RW
 *   IMPORTANT: Non-shareable (MPU_RACR_S is omitted).
 *   On Cortex-R5F without an external exclusive monitor, marking DDR
 *   Shareable causes LDREX/STREX instructions to trigger a Data Abort.
 *   Non-shareable forces the core to use its internal local monitor.
 */

#define am67a_mcu_ddr_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_TEX(1) | \
                                            MPU_RACR_C      | \
                                            MPU_RACR_B      | \
                                            MPU_RACR_AP_RWRW)

/* Region 6 & 7: Linux RemoteProc IPC VRing and Resource Table
 *   Normal, Non-cached, Shareable, Execute Never, Full RW
 */

#define am67a_mcu_linux_vring_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_TEX(1) | \
                                            MPU_RACR_S      | \
                                            MPU_RACR_AP_RWRW | \
                                            MPU_RACR_XN)

#define am67a_mcu_linux_rsctbl_region(r, base, size) \
  am67a_mcu_mpu_configure_region(r, base, size, MPU_RACR_TEX(1) | \
                                            MPU_RACR_S      | \
                                            MPU_RACR_AP_RWRW | \
                                            MPU_RACR_XN)

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: am67a_mcu_mpu_configure_region
 *
 * Description:
 *   Configure a specific MPU region by index with given base, size, and
 *   attributes.
 *
 ****************************************************************************/

void am67a_mcu_mpu_configure_region(uint32_t region, uintptr_t base,
                                size_t size, uint32_t flags);

/****************************************************************************
 * Name: am67a_mcu_mpu_reset
 *
 * Description:
 *   Reset all MPU regions by clearing DRBAR, DRSR, and DRACR for all
 *   available regions in the Cortex-R5F.
 *
 ****************************************************************************/

void am67a_mcu_mpu_reset(void);

/****************************************************************************
 * Name: am67a_mcu_mpu_init
 *
 * Description:
 *   Initialize the Cortex-R5F MPU with default memory regions for the
 *   TI AM67A_MCU MCU domain.
 *
 ****************************************************************************/

void am67a_mcu_mpu_init(void);

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_MPUINIT_H */
