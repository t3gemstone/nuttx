/****************************************************************************
 * arch/arm/src/am67/am67_dshot.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_DSHOT_H
#define __ARCH_ARM_SRC_AM67_AM67_DSHOT_H

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef CONFIG_AM67_DSHOT

/* DShot on the four EPWM motor outputs (EPWM0 A/B, EPWM1 A/B).
 * Channel n is output n: 0 = EPWM0_A, 1 = EPWM0_B, 2 = EPWM1_A,
 * 3 = EPWM1_B.
 */

#define AM67_DSHOT_NCHANNELS 4

/* Take the EPWM modules and install the FIQ bit engine.
 * bitrate: 150000, 300000 or 600000.  Channels outside channel_mask stay
 * low.  Returns the channel mask in use or a negated errno.
 */

int am67_dshot_init(uint32_t bitrate, uint32_t channel_mask);

/* Stage an 11-bit value (0 stop, 1..47 commands, 48..2047 throttle) and
 * the telemetry request bit for the next frame.
 */

void am67_dshot_set(unsigned channel, uint16_t value, bool telemetry);

/* Send the staged values as one frame on all channels and return when it
 * is out (one frame time, IRQs masked meanwhile).  Returns -EBUSY if a
 * frame is still being sent, -ETIMEDOUT if the FIQ engine stalled (the
 * pins are then held low).
 */

int am67_dshot_trigger(void);

/* While disarmed no frame is sent and the outputs stay low. */

void am67_dshot_arm(bool armed);

struct am67_dshot_stats_s
{
  uint32_t frames;          /* frames sent */
  uint32_t overruns;        /* triggers refused, previous frame busy */
  uint32_t late;            /* frames dropped, FIQ overran a bit period
                             * (includes skipped) */
  uint32_t skipped;         /* frames dropped, FIQ came a period late */
  uint32_t slow;            /* rows written past half a bit period */
  uint32_t timeouts;        /* frames stopped, the FIQ did not finish */
  uint32_t max_exit_ticks;  /* worst FIQ end, TBCLK ticks after CTR = 0 */
  uint32_t max_fiq_cycles;  /* worst FIQ run time, CPU cycles */
  uint32_t frame_cycles;    /* last frame, bit 0 FIQ to last FIQ, cycles */
  uint32_t period_ticks;    /* bit period, TBCLK ticks */
};

void am67_dshot_stats(struct am67_dshot_stats_s *stats);

#endif /* CONFIG_AM67_DSHOT */
#endif /* __ARCH_ARM_SRC_AM67_AM67_DSHOT_H */
