/****************************************************************************
 * arch/arm/src/am67/am67_rti.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_RTI_H
#define __ARCH_ARM_SRC_AM67_AM67_RTI_H

#include <nuttx/config.h>
#include <stdint.h>

#ifdef CONFIG_AM67_RTI8_WDT

/* Post-mortem block g_am67_wdt_fiq_info: [0] magic, [1] pc, [2] lr,
 * [3] cpsr, [4] sp of the hung context, [5] VIM ACTFIQ (bit 31 valid,
 * bits 9:0 input; 30 = RTI8).
 */

#define AM67_WDT_INFO_WORDS  8
#define AM67_WDT_INFO_MAGIC  0x57445446u   /* "FTDW" */

void am67_wdt_info_set(int index, uint32_t value);

/* Enable the RTI8 watchdog (NMI reaction, 100 % window) and kick it.
 * Returns the effective timeout in ms, which is the previous image's if
 * the watchdog was already running, or a negated errno.
 */

int am67_rti_wdt_start(uint32_t timeout_ms);

/* Restart the countdown. */

void am67_rti_wdt_kick(void);

/* Current down counter and violation flags, for diagnostics. */

uint32_t am67_rti_wdt_counter(void);
uint32_t am67_rti_wdt_status(void);

/* Route the expiry to the FIQ that cuts the motors.  Call only once the
 * kicks are running, since it takes effect at the next expiry.
 */

void am67_rti_wdt_arm_fiq(void);

#endif /* CONFIG_AM67_RTI8_WDT */
#endif /* __ARCH_ARM_SRC_AM67_AM67_RTI_H */
