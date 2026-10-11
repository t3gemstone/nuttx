/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_pwm.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_PWM_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_PWM_H

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

/* Hardware Instances */

#define AM67A_MCU_EPWM0_BASE           0x23000000ul
#define AM67A_MCU_EPWM1_BASE           0x23010000ul
#define AM67A_MCU_ECAP0_BASE           0x23100000ul
#define AM67A_MCU_ECAP1_BASE           0x23110000ul
#define AM67A_MCU_ECAP2_BASE           0x23120000ul

#define AM67A_MCU_EPWM_CLK_FREQ        250000000ul /* 250 MHz DEV_EPWMx_VBUSP_CLK */
#define AM67A_MCU_ECAP_CLK_FREQ        125000000ul /* 125 MHz DEV_ECAPx_VBUS_CLK  */

#define AM67A_MCU_PWM_NCHANNELS        7

/* Channel identifiers:
 *   CH0: EPWM0 Sub-channel A (Hat Pin 29)
 *   CH1: EPWM0 Sub-channel B (Hat Pin 8)
 *   CH2: EPWM1 Sub-channel A (Hat Pin 31)
 *   CH3: EPWM1 Sub-channel B (Hat Pin 33)
 *   CH4: ECAP0 APWM          (Hat Pin 32)
 *   CH5: ECAP1 APWM          (Hat Pin 36)
 *   CH6: ECAP2 APWM          (Hat Pin 12)
 */

#define AM67A_MCU_PWM_CH0              0
#define AM67A_MCU_PWM_CH1              1
#define AM67A_MCU_PWM_CH2              2
#define AM67A_MCU_PWM_CH3              3
#define AM67A_MCU_PWM_CH4              4
#define AM67A_MCU_PWM_CH5              5
#define AM67A_MCU_PWM_CH6              6

/* EHRPWM Register Offsets (16-bit registers) */

#define AM67A_EPWM_TBCTL_OFFSET        0x00u /* Time-Base Control */
#define AM67A_EPWM_TBSTS_OFFSET        0x02u /* Time-Base Status */
#define AM67A_EPWM_TBPHS_OFFSET        0x04u /* Time-Base Phase */
#define AM67A_EPWM_TBCNT_OFFSET        0x08u /* Time-Base Counter */
#define AM67A_EPWM_TBPRD_OFFSET        0x0au /* Time-Base Period */
#define AM67A_EPWM_CMPCTL_OFFSET       0x0eu /* Counter-Compare Control */
#define AM67A_EPWM_CMPA_OFFSET         0x12u /* Counter-Compare A */
#define AM67A_EPWM_CMPB_OFFSET         0x14u /* Counter-Compare B */
#define AM67A_EPWM_AQCTLA_OFFSET       0x16u /* Action Qualifier Control A */
#define AM67A_EPWM_AQCTLB_OFFSET       0x18u /* Action Qualifier Control B */
#define AM67A_EPWM_AQSFRC_OFFSET       0x1au /* Action Qualifier Software Force */
#define AM67A_EPWM_AQCSFRC_OFFSET      0x1cu /* Action Qualifier Cont. Software Force */

/* ECAP Register Offsets */

#define AM67A_ECAP_TSCTR_OFFSET        0x00u /* 32-bit Time-Stamp Counter */
#define AM67A_ECAP_CTRPHS_OFFSET       0x04u /* 32-bit Counter Phase */
#define AM67A_ECAP_CAP1_OFFSET         0x08u /* 32-bit Period active */
#define AM67A_ECAP_CAP2_OFFSET         0x0cu /* 32-bit Compare active */
#define AM67A_ECAP_CAP3_OFFSET         0x10u /* 32-bit Period shadow */
#define AM67A_ECAP_CAP4_OFFSET         0x14u /* 32-bit Compare shadow */
#define AM67A_ECAP_ECCTL2_OFFSET       0x2au /* 16-bit Control 2 */

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

/****************************************************************************
 * Name: am67a_mcu_pwm_init
 *
 * Description:
 *   Initialize the AM67A ePWM hardware subsystem (pinmux, clocks, registers).
 *
 * Returned Value:
 *   OK (0) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int am67a_mcu_pwm_init(void);

/****************************************************************************
 * Name: am67a_mcu_pwm_set_freq
 *
 * Description:
 *   Set the PWM frequency for a specific channel's ePWM module.
 *   Note: CH0 and CH1 share EPWM0 frequency; CH2 and CH3 share EPWM1.
 *
 * Input Parameters:
 *   channel - Channel index (0 to 3)
 *   freq_hz - Frequency in Hertz (e.g. 50 to 400)
 *
 * Returned Value:
 *   OK (0) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int am67a_mcu_pwm_set_freq(int channel, uint32_t freq_hz);

/****************************************************************************
 * Name: am67a_mcu_pwm_set_duty_us
 *
 * Description:
 *   Set the pulse width (duty duration) in microseconds for a channel.
 *   Standard ESC range: 1000 us (0% throttle) to 2000 us (100% throttle).
 *   Setting pulse_us to 0 safely stops pulse generation and forces low.
 *
 * Input Parameters:
 *   channel  - Channel index (0 to 3)
 *   pulse_us - Active high pulse duration in microseconds
 *
 * Returned Value:
 *   OK (0) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int am67a_mcu_pwm_set_duty_us(int channel, uint32_t pulse_us);

/****************************************************************************
 * Name: am67a_mcu_pwm_stop
 *
 * Description:
 *   Safely stop PWM output on the specified channel, forcing output LOW.
 *
 * Input Parameters:
 *   channel - Channel index (0 to 3)
 *
 * Returned Value:
 *   OK (0) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int am67a_mcu_pwm_stop(int channel);

/****************************************************************************
 * Name: am67a_mcu_pwm_stop_all
 *
 * Description:
 *   Safely stop all 4 PWM outputs, forcing outputs LOW.
 *
 ****************************************************************************/

void am67a_mcu_pwm_stop_all(void);

#ifdef CONFIG_PWM
/****************************************************************************
 * Name: am67a_mcu_pwminit
 *
 * Description:
 *   Initialize an ePWM instance and return a pointer to the lower half
 *   PWM device structure for registration with NuttX pwm_register().
 *
 * Input Parameters:
 *   timer - ePWM module index (0 for EPWM0, 1 for EPWM1)
 *
 * Returned Value:
 *   Pointer to lower-half PWM structure, or NULL on failure.
 *
 ****************************************************************************/

FAR struct pwm_lowerhalf_s *am67a_mcu_pwminit(int timer);
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_PWM_H */
