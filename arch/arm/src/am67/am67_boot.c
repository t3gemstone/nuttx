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

#include "am67_boot.h"

#include <arch/board/board.h>
#include <assert.h>
#include <debug.h>
#include <nuttx/config.h>
#include <nuttx/init.h>
#include <stdint.h>

#include "am67_clockconfig.h"
#include "am67_irq.h"
#include "am67_lowput.h"
#include "am67_mpuinit.h"
#include "am67_rsc.h"
#include "am67_serial.h"
#include "arm.h"
#include "arm_internal.h"
#include "chip.h"
#include "mpu.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_boot
 *
 * Description:
 *   Complete boot operations started in arm_head.S
 *
 * Boot Sequence
 *
 *   1.  The __start entry point in armv7-r/arm_head.S is invoked upon power-
 *       on reset.
 *   2.  __start prepares CPU for code execution.
 *   3. If CONFIG_ARMV7R_MEMINIT is not defined, then __start will prepare
 *       memory resources by calling arm_data_initialize() and will then
 *       branch this function.
 *   4.  This function will then branch to nx_start() to start the operating
 *       system.
 *
 ****************************************************************************/



__attribute__((section(".resource_table")))
volatile const struct resource_table rsc_table = {
    .ver = 1,
    .num = 1,  /* One resource entry (Virtio device) */
    .reserved = {0, 0},
    .offset = {
        offsetof(struct resource_table, vdev), /* Offset to Virtio device */
    },
    .vdev = {
        .type = RSC_VDEV,
        .id = VIRTIO_ID_RPMSG,
        .notifyid = 0,
        .dfeatures = 0,
        .gfeatures = 0,
        .config_len = 0,
        .status = 0,
        .num_of_vrings = 2,
        .reserved = {0, 0},
        .vring = {
            {SHARED_MEM_BASE, 4096, 512, 0, 0}, /* VRING0: RX */
            {SHARED_MEM_BASE + 0x8000, 4096, 512, 0, 0}, /* VRING1: TX */
        },
    },
};


void arm_boot(void)
{
    am67_mpu_initialize();
    am67_lowsetup();

    /* Then start NuttX */
    nx_start();
}
