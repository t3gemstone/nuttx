 /****************************************************************************
 * arch/arm/src/am67/am67_mpuinit.c
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
#include <assert.h>
#include <stdint.h>
#include <sys/param.h>
#include <nuttx/userspace.h>
#include <arch/barriers.h>
#include "mpu.h"
#include "hardware/am67_memorymap.h"
#include "am67_mpuinit.h"
#include "arm_internal.h"
#include "am67_irq.h"

/****************************************************************************
 * Private Function 
 ****************************************************************************/

/****************************************************************************
 * Name: am67_mpu_reset
 * Description:
 * It sets the region to zero to clear it. Zero means the default value of the region attributes; you will need to set them as desired afterward.
 ****************************************************************************/
void am67_mpu_reset(){
  for( int i =0 ; i < NUM_OF_REGION ; i++){
    mpu_set_region_zero(i);
  }
  
}

/****************************************************************************
 * Public Function 
 ****************************************************************************/

/****************************************************************************
 * Name: am67_mpu_initialize
 *
 * Description:
 *   Configure the MPU according to the given region. The regions are: ATCM, BTCM, MSRAM, and DDR.
 *
 ****************************************************************************/
void am67_mpu_initialize(void)
{

  /*turn off the MPU*/

  mpu_control(false);

  /*Disable Background Region*/

  am67_mpu_disableBR();

  /* Clear region's attributes and set to 0 */
  am67_mpu_reset();

  /*Set regions according to given attributes */

  am67_register_region(ARM67_REGISTER_START_ADDR,ARM67_REGISTER_SIZE);
  
  am67_tcma_region(ARM67_TCMA_START_ADDR,ARM67_TCMA_SIZE);

  am67_tcmb_region(ARM67_TCMB_START_ADDR, ARM67_TCMB_SIZE);
  
  am67_mcu_msram_region(ARM67_MCU_MSRAM_START_ADDR, ARM67_MCU_MSRAM_SIZE);
  
  am67_ddr_region(ARM67_DDR_START_ADDR, ARM67_DDR_SIZE);

  /*turn on the MPU*/

  mpu_control(true);
  
  return;
}

