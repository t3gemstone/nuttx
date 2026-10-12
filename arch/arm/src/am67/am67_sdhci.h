/****************************************************************************
 * arch/arm/src/am67/am67_sdhci.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_SDHCI_H
#define __ARCH_ARM_SRC_AM67_AM67_SDHCI_H

#include <nuttx/config.h>

#ifdef CONFIG_AM67_SDHCI0

struct sdio_dev_s;

/* MMCSD0 (the on-board eMMC) as an SDIO lower half for drivers/mmcsd.
 * Powers the controller over TISCI and resets it.  Returns NULL if the
 * controller does not answer.  Pass the result to mmcsd_slotinitialize().
 */

struct sdio_dev_s *am67_sdhci0_initialize(void);

#ifdef CONFIG_AM67_FAULT_INJECTION
/* Make the next count data transfers fail: mode 1 stalls them (no data
 * moved, the wait times out), mode 2 reports an error at transfer
 * complete. Bench tests only.
 */

void am67_sdhci_inject_fault(unsigned int mode, unsigned int count);
#endif

#endif /* CONFIG_AM67_SDHCI0 */
#endif /* __ARCH_ARM_SRC_AM67_AM67_SDHCI_H */
