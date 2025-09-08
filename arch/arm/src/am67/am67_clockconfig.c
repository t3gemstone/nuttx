/* Copyright (C) 2021 Texas Instruments Incorporated */
/****************************************************************************
 * arch/arm/src/am67/am67_clockconfig.c
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
#include <stdio.h>

#include "arm_internal.h"
#include "am67_clockconfig.h"
#include "hardware/am67_memorymap.h"
#include <nuttx/arch.h>
#include "am67_timer.h"
#include "am67_irq.h"
#include "irq/irq.h"


/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#include <stdint.h>





void am67_clockconfig(){
    /* all periphal clocks are setted by U-boot so we don't need to setup*/
}
