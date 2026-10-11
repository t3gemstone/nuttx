/****************************************************************************
 * arch/arm/include/am67a_mcu/am67a_mcu_pwm.h
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

#ifndef __ARCH_ARM_INCLUDE_AM67A_MCU_AM67A_MCU_PWM_H
#define __ARCH_ARM_INCLUDE_AM67A_MCU_AM67A_MCU_PWM_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef CONFIG_PWM
#  include <nuttx/timers/pwm.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AM67A_MCU_PWM_NCHANNELS        7

#define AM67A_MCU_PWM_CH0              0
#define AM67A_MCU_PWM_CH1              1
#define AM67A_MCU_PWM_CH2              2
#define AM67A_MCU_PWM_CH3              3
#define AM67A_MCU_PWM_CH4              4
#define AM67A_MCU_PWM_CH5              5
#define AM67A_MCU_PWM_CH6              6

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

int am67a_mcu_pwm_init(void);
int am67a_mcu_pwm_set_freq(int channel, uint32_t freq_hz);
int am67a_mcu_pwm_set_duty_us(int channel, uint32_t pulse_us);
int am67a_mcu_pwm_stop(int channel);
void am67a_mcu_pwm_stop_all(void);

#ifdef CONFIG_PWM
FAR struct pwm_lowerhalf_s *am67a_mcu_pwminit(int timer);
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __ARCH_ARM_INCLUDE_AM67A_MCU_AM67A_MCU_PWM_H */
