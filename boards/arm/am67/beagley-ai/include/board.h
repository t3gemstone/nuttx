/****************************************************************************
 * boards/arm/am67/beagley-ai/include/board.h
 *
 * Author: Emre CECANPUNAR <emreleno@gmail.com>
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

#ifndef __BOARDS_ARM_AM67_BEAGLEY_AI_INCLUDE_BOARD_H
#define __BOARDS_ARM_AM67_BEAGLEY_AI_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Clocking *****************************************************************/

/* Since NuttX is booted from a loader on the AM67, clocking should already
 * be setup when NuttX starts.
 */


/****************************************************************************
 * Assembly Language Macros
 ****************************************************************************/

#ifdef __ASSEMBLY__
  .macro  config_sdram
  .endm
#endif /* __ASSEMBLY__ */

#endif /* __BOARDS_ARM_AM67_BEAGLEY_AI_INCLUDE_BOARD_H */
