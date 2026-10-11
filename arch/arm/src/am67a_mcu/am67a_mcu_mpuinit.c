/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_mpuinit.c
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
#include <sys/types.h>

#include "sctlr.h"
#include "mpu.h"
#include "am67a_mcu_mpuinit.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_mpu_disable_br
 *
 * Description:
 *   Disable the MPU background region by clearing bit 17 (BR) in the SCTLR.
 *
 ****************************************************************************/

static inline void am67a_mcu_mpu_disable_br(void)
{
  uint32_t sctlr = cp15_rdsctlr();

  sctlr &= ~AM67A_MCU_SCTLR_BR;
  cp15_wrsctlr(sctlr);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_mpu_configure_region
 *
 * Description:
 *   Configure a specific MPU region by index with given base, size, and
 *   attributes.
 *
 ****************************************************************************/

void am67a_mcu_mpu_configure_region(uint32_t region, uintptr_t base,
                                size_t size, uint32_t flags)
{
  uint32_t regval;
  uint8_t l2size;
  uint32_t subregions;

  /* Select the region */

  mpu_set_rgnr(region);

  /* Set region base address */

  mpu_set_drbar(base & MPU_RBAR_ADDR_MASK);

  /* Calculate region size and subregions */

  l2size = mpu_log2regionceil(size);
  subregions = mpu_subregion(base, size, l2size);

  /* Set region access control */

  mpu_set_dracr(flags);

  /* Enable region with calculated size and subregion mask */

  regval = MPU_RASR_ENABLE |
           MPU_RASR_RSIZE_LOG2(l2size) |
           (subregions << MPU_RASR_SRD_SHIFT);
  mpu_set_drsr(regval);
}

/****************************************************************************
 * Name: am67a_mcu_mpu_reset
 *
 * Description:
 *   Reset all MPU regions by clearing DRBAR, DRSR, and DRACR for all
 *   available regions in the Cortex-R5F.
 *
 ****************************************************************************/

void am67a_mcu_mpu_reset(void)
{
  int i;

  for (i = 0; i < AM67A_MCU_NUM_OF_MPU_REGIONS; i++)
    {
      mpu_set_rgnr(i);
      mpu_set_drbar(0);
      mpu_set_dracr(0);
      mpu_set_drsr(0);
    }
}

/****************************************************************************
 * Name: am67a_mcu_mpu_init
 *
 * Description:
 *   Initialize the Cortex-R5F MPU with default memory regions for the
 *   TI AM67A_MCU MCU domain.
 *
 ****************************************************************************/

void am67a_mcu_mpu_init(void)
{
  /* Configure regions in order of priority (higher index = higher
   * priority).
   * Note: We do not disable the MPU or reset regions here because
   * the CPU is currently executing code from DDR SDRAM, which was
   * mapped by the TCMA early boot trampoline.
   */

  /* 4. Configure regions in order of priority (higher index = higher
   *    priority)
   *
   * Region 0: 0x00000000 (2GB) Device MMR / Registers, XN
   * Region 1: 0x00000000 (32KB) TCMA, Non-cached, RWX
   * Region 2: 0x41010000 (32KB) TCMB, Non-cached, RWX
   * Region 3: 0x79100000 (512KB) MCU MSRAM, Cacheable WB/WA, RWX
   * Region 4: 0x60000000 (64MB) Flash (OSPI), Cacheable WB, RWX
   * Region 5: 0x80000000 (2GB) DDR RAM, Cacheable WB/WA, Non-shareable, RWX
   * Region 6: 0xA1000000 (1MB) Linux IPC VRing, Non-cached, Shareable, XN
   * Region 7: 0xA1100000 (1MB) Linux IPC Resource Table/Trace, Non-cached,
   *           Shareable, XN
   */

  /* Configure DDR (Region 5) FIRST so that executing code from DDR SDRAM
   * is never interrupted or unmapped during region configuration.
   */

  am67a_mcu_ddr_region(5, AM67A_MCU_DDR_START_ADDR,
                   AM67A_MCU_DDR_SIZE);
  am67a_mcu_register_region(0, AM67A_MCU_REGISTER_START_ADDR,
                        AM67A_MCU_REGISTER_SIZE);
  am67a_mcu_tcma_region(1, AM67A_MCU_TCMA_START_ADDR,
                    AM67A_MCU_TCMA_SIZE);
  am67a_mcu_tcmb_region(2, AM67A_MCU_TCMB_START_ADDR,
                    AM67A_MCU_TCMB_SIZE);
  am67a_mcu_mcu_msram_region(3, AM67A_MCU_MCU_MSRAM_START_ADDR,
                         AM67A_MCU_MCU_MSRAM_SIZE);
  am67a_mcu_flash_region(4, AM67A_MCU_FLASH_START_ADDR,
                     AM67A_MCU_FLASH_SIZE);
  am67a_mcu_linux_vring_region(6, AM67A_MCU_LINUX_VRING_START_ADDR,
                           AM67A_MCU_LINUX_VRING_SIZE);
  am67a_mcu_linux_rsctbl_region(7, AM67A_MCU_LINUX_RSCTBL_START_ADDR,
                            AM67A_MCU_LINUX_RSCTBL_SIZE);

  /* 5. Enable the MPU */

  mpu_control(true);
}
