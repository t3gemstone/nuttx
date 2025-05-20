/****************************************************************************
 * arch/arm/src/am67/am67_gpio.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_GPIO_H
#define __ARCH_ARM_SRC_AM67_AM67_GPIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "arm_internal.h"

#include "hardware/am67_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/**
 *  \anchor GPIO_PinValue_t
 *  \name GPIO Pin Value
 *  @{
 */
/** \brief GPIO pin is at logic low */
#define GPIO_PIN_LOW                    (0U)
/** \brief GPIO pin is at logic high */
#define GPIO_PIN_HIGH                   (1U)
/** @} */

/**
 *  \anchor GPIO_Direction_t
 *  \name GPIO Direction
 *  @{
 */
#define GPIO_DIRECTION_OUTPUT           (0U)
#define GPIO_DIRECTION_INPUT            (1U)
/** @} */

/**
 *  \anchor GPIO_TrigType_t
 *  \name GPIO Trigger Type
 *  @{
 */
/** \brief No interrupt request on either rising or falling edges on the pin */
#define GPIO_TRIG_TYPE_NONE             (0U)
/** \brief Interrupt request on occurrence of a rising edge on the input pin */
#define GPIO_TRIG_TYPE_RISE_EDGE        (1U)
/** \brief Interrupt request on occurrence of a falling edge on the input pin */
#define GPIO_TRIG_TYPE_FALL_EDGE        (2U)
/** \brief Interrupt request on occurrence of a rising/falling edge on the input pin */
#define GPIO_TRIG_TYPE_BOTH_EDGE        (3U)
/** @} */


/** \brief Maximum number of banks per instance/module */
#define GPIO_MAX_BANKS                  (9U)
/** \brief Maximum number of pins per bank */
#define GPIO_MAX_PIN_PER_BANK           (16U)
/** \brief Maximum number of pins per instance/module */
#define GPIO_MAX_PIN_PER_INSTANCE       (GPIO_MAX_BANKS * GPIO_MAX_PIN_PER_BANK)

/** \brief Number of banks per register */
#define GPIO_BANKS_PER_REG              (2U)
/** \brief Number of pins per register - 32 pins */
#define GPIO_PINS_PER_REG               (GPIO_BANKS_PER_REG * GPIO_MAX_PIN_PER_BANK)

/** \brief Number of pins per register - shift value - used instead of divide operator */
#define GPIO_PINS_PER_REG_SHIFT         (5U)
/** \brief Number of pins per bank - shift value - used instead of divide operator */
#define GPIO_PINS_PER_BANK_SHIFT        (4U)

/** \brief Returns the bank index based on pin number */
#define GPIO_GET_BANK_INDEX(pinNum)     (((uint32_t) pinNum) >> GPIO_PINS_PER_BANK_SHIFT)
/** \brief Returns the register index based on pin number */
#define GPIO_GET_REG_INDEX(pinNum)      (((uint32_t) pinNum) >> GPIO_PINS_PER_REG_SHIFT)
/** \brief Returns the bit position within a register based on pin number */
#define GPIO_GET_BIT_POS(pinNum)        (pinNum - ((GPIO_GET_REG_INDEX(pinNum)) << GPIO_PINS_PER_REG_SHIFT))
/** \brief Returns the bit mask within a register based on pin number */
#define GPIO_GET_BIT_MASK(pinNum)       (((uint32_t) 1U) << GPIO_GET_BIT_POS(pinNum))
/** \brief Returns the bit position within a bank based on pin number */
#define GPIO_GET_BANK_BIT_POS(pinNum)   (pinNum - ((GPIO_GET_BANK_INDEX(pinNum)) << GPIO_PINS_PER_BANK_SHIFT))
/** \brief Returns the bit mask within a bank based on pin number */
#define GPIO_GET_BANK_BIT_MASK(pinNum)  (((uint32_t) 1U) << GPIO_GET_BANK_BIT_POS(pinNum))

typedef struct {
    volatile uint32_t DIR;
    volatile uint32_t OUT_DATA;
    volatile uint32_t SET_DATA;
    volatile uint32_t CLR_DATA;
    volatile uint32_t IN_DATA;
    volatile uint32_t SET_RIS_TRIG;
    volatile uint32_t CLR_RIS_TRIG;
    volatile uint32_t SET_FAL_TRIG;
    volatile uint32_t CLR_FAL_TRIG;
    volatile uint32_t INTSTAT;
} CSL_GpioBank_registersRegs;


typedef struct {
    volatile uint32_t PID;
    volatile uint32_t PCR;
    volatile uint32_t BINTEN;
    volatile uint8_t  RSVD0[4];
    CSL_GpioBank_registersRegs  BANK_REGISTERS[9];
} CSL_GpioRegs;

inline void CSL_REG32_WR_RAW(volatile uint32_t * const p, uint32_t v);

inline void CSL_REG32_WR_RAW(volatile uint32_t * const p, uint32_t v)
{
    *p = v;
    return;
}

/**
 *  \brief   This macro writes a 32-bit value to a hardware register.
 *
 *  \param   p       Address of the memory mapped hardware register.
 *  \param   v       Unsigned 32-bit value which has to be written to the
 *                   register.
 */
#define CSL_REG32_WR(p, v)      (CSL_REG32_WR_RAW( \
                                    (volatile uint32_t *) (p), (uint32_t) (v)))
/* the Field MaKe (Raw) macro */
#define CSL_FMKR(msb, lsb, val)                                             \
    (((val) & ((((uint32_t)1U) << ((msb) - (lsb) + ((uint32_t)1U))) - ((uint32_t)1U))) << (lsb))
/* the Field INSert (Raw) macro */
#define CSL_FINSR(reg, msb, lsb, val)                                       \
    ((reg) = ((reg) & (~(((((uint32_t)1U) << ((msb) - (lsb) + ((uint32_t)1U))) - ((uint32_t)1U)) << (lsb))))         \
    | CSL_FMKR((msb), (lsb), (val)))
/* DIR */

#define CSL_GPIO_DIR_DIR_SHIFT                                  (0U)
#define CSL_GPIO_DIR_DIR_MASK                                   (0x0000FFFFU)
#define CSL_GPIO_DIR_DIR_RESETVAL                               (0x00000000U)
#define CSL_GPIO_DIR_DIR_MAX                                    (0x0000ffffU)

#define CSL_GPIO_DIR_RESETVAL                                   (0x00000000U)

/* OUT_DATA */

#define CSL_GPIO_OUT_DATA_OUT_SHIFT                             (0U)
#define CSL_GPIO_OUT_DATA_OUT_MASK                              (0x0000FFFFU)
#define CSL_GPIO_OUT_DATA_OUT_RESETVAL                          (0x00000000U)
#define CSL_GPIO_OUT_DATA_OUT_MAX                               (0x0000ffffU)

#define CSL_GPIO_OUT_DATA_RESETVAL                              (0x00000000U)

/* SET_DATA */

#define CSL_GPIO_SET_DATA_SET_SHIFT                             (0U)
#define CSL_GPIO_SET_DATA_SET_MASK                              (0x0000FFFFU)
#define CSL_GPIO_SET_DATA_SET_RESETVAL                          (0x00000000U)
#define CSL_GPIO_SET_DATA_SET_MAX                               (0x0000ffffU)

#define CSL_GPIO_SET_DATA_RESETVAL                              (0x00000000U)

/* CLR_DATA */

#define CSL_GPIO_CLR_DATA_CLR_SHIFT                             (0U)
#define CSL_GPIO_CLR_DATA_CLR_MASK                              (0x0000FFFFU)
#define CSL_GPIO_CLR_DATA_CLR_RESETVAL                          (0x00000000U)
#define CSL_GPIO_CLR_DATA_CLR_MAX                               (0x0000ffffU)

#define CSL_GPIO_CLR_DATA_RESETVAL                              (0x00000000U)

/* IN_DATA */

#define CSL_GPIO_IN_DATA_IN_SHIFT                               (0U)
#define CSL_GPIO_IN_DATA_IN_MASK                                (0x0000FFFFU)
#define CSL_GPIO_IN_DATA_IN_RESETVAL                            (0x00000000U)
#define CSL_GPIO_IN_DATA_IN_MAX                                 (0x0000ffffU)

#define CSL_GPIO_IN_DATA_RESETVAL                               (0x00000000U)

/* SET_RIS_TRIG */

#define CSL_GPIO_SET_RIS_TRIG_SETRIS_SHIFT                      (0U)
#define CSL_GPIO_SET_RIS_TRIG_SETRIS_MASK                       (0x0000FFFFU)
#define CSL_GPIO_SET_RIS_TRIG_SETRIS_RESETVAL                   (0x00000000U)
#define CSL_GPIO_SET_RIS_TRIG_SETRIS_MAX                        (0x0000ffffU)

#define CSL_GPIO_SET_RIS_TRIG_RESETVAL                          (0x00000000U)

/* CLR_RIS_TRIG */

#define CSL_GPIO_CLR_RIS_TRIG_CLRRIS_SHIFT                      (0U)
#define CSL_GPIO_CLR_RIS_TRIG_CLRRIS_MASK                       (0x0000FFFFU)
#define CSL_GPIO_CLR_RIS_TRIG_CLRRIS_RESETVAL                   (0x00000000U)
#define CSL_GPIO_CLR_RIS_TRIG_CLRRIS_MAX                        (0x0000ffffU)

#define CSL_GPIO_CLR_RIS_TRIG_RESETVAL                          (0x00000000U)

/* SET_FAL_TRIG */

#define CSL_GPIO_SET_FAL_TRIG_SETFAL_SHIFT                      (0U)
#define CSL_GPIO_SET_FAL_TRIG_SETFAL_MASK                       (0x0000FFFFU)
#define CSL_GPIO_SET_FAL_TRIG_SETFAL_RESETVAL                   (0x00000000U)
#define CSL_GPIO_SET_FAL_TRIG_SETFAL_MAX                        (0x0000ffffU)

#define CSL_GPIO_SET_FAL_TRIG_RESETVAL                          (0x00000000U)

/* CLR_FAL_TRIG */

#define CSL_GPIO_CLR_FAL_TRIG_CLRFAL_SHIFT                      (0U)
#define CSL_GPIO_CLR_FAL_TRIG_CLRFAL_MASK                       (0x0000FFFFU)
#define CSL_GPIO_CLR_FAL_TRIG_CLRFAL_RESETVAL                   (0x00000000U)
#define CSL_GPIO_CLR_FAL_TRIG_CLRFAL_MAX                        (0x0000ffffU)

#define CSL_GPIO_CLR_FAL_TRIG_RESETVAL                          (0x00000000U)

/* INTSTAT */

#define CSL_GPIO_INTSTAT_STAT_SHIFT                             (0U)
#define CSL_GPIO_INTSTAT_STAT_MASK                              (0x0000FFFFU)
#define CSL_GPIO_INTSTAT_STAT_RESETVAL                          (0x00000000U)
#define CSL_GPIO_INTSTAT_STAT_MAX                               (0x0000ffffU)

#define CSL_GPIO_INTSTAT_RESETVAL                               (0x00000000U)

/* PID */

#define CSL_GPIO_PID_MINOR_SHIFT                                (0U)
#define CSL_GPIO_PID_MINOR_MASK                                 (0x0000003FU)
#define CSL_GPIO_PID_MINOR_RESETVAL                             (0x00000000U)
#define CSL_GPIO_PID_MINOR_MAX                                  (0x0000003fU)

#define CSL_GPIO_PID_CUSTOM_SHIFT                               (6U)
#define CSL_GPIO_PID_CUSTOM_MASK                                (0x000000C0U)
#define CSL_GPIO_PID_CUSTOM_RESETVAL                            (0x00000000U)
#define CSL_GPIO_PID_CUSTOM_MAX                                 (0x00000003U)

#define CSL_GPIO_PID_MAJOR_SHIFT                                (8U)
#define CSL_GPIO_PID_MAJOR_MASK                                 (0x00000700U)
#define CSL_GPIO_PID_MAJOR_RESETVAL                             (0x00000000U)
#define CSL_GPIO_PID_MAJOR_MAX                                  (0x00000007U)

#define CSL_GPIO_PID_RTL_SHIFT                                  (11U)
#define CSL_GPIO_PID_RTL_MASK                                   (0x0000F800U)
#define CSL_GPIO_PID_RTL_RESETVAL                               (0x00000000U)
#define CSL_GPIO_PID_RTL_MAX                                    (0x0000001fU)

#define CSL_GPIO_PID_FUNC_SHIFT                                 (16U)
#define CSL_GPIO_PID_FUNC_MASK                                  (0x0FFF0000U)
#define CSL_GPIO_PID_FUNC_RESETVAL                              (0x00000000U)
#define CSL_GPIO_PID_FUNC_MAX                                   (0x00000fffU)

#define CSL_GPIO_PID_SCHEME_SHIFT                               (30U)
#define CSL_GPIO_PID_SCHEME_MASK                                (0xC0000000U)
#define CSL_GPIO_PID_SCHEME_RESETVAL                            (0x00000000U)
#define CSL_GPIO_PID_SCHEME_MAX                                 (0x00000003U)

#define CSL_GPIO_PID_RESETVAL                                   (0x00000000U)

/* PCR */

#define CSL_GPIO_PCR_FREE_SHIFT                                 (0U)
#define CSL_GPIO_PCR_FREE_MASK                                  (0x00000001U)
#define CSL_GPIO_PCR_FREE_RESETVAL                              (0x00000000U)
#define CSL_GPIO_PCR_FREE_MAX                                   (0x00000001U)

#define CSL_GPIO_PCR_SOFT_SHIFT                                 (1U)
#define CSL_GPIO_PCR_SOFT_MASK                                  (0x00000002U)
#define CSL_GPIO_PCR_SOFT_RESETVAL                              (0x00000000U)
#define CSL_GPIO_PCR_SOFT_MAX                                   (0x00000001U)

#define CSL_GPIO_PCR_RESETVAL                                   (0x00000000U)

/* BINTEN */

#define CSL_GPIO_BINTEN_EN_SHIFT                                (0U)
#define CSL_GPIO_BINTEN_EN_MASK                                 (0x000000FFU)
#define CSL_GPIO_BINTEN_EN_RESETVAL                             (0x00000000U)
#define CSL_GPIO_BINTEN_EN_MAX                                  (0x000000ffU)

#define CSL_GPIO_BINTEN_RESETVAL                                (0x00000000U)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Name: am67_gpio_set_func
 *
 * Description:
 *   Set the specified GPIO pin to be input, output or use one of its
 *   alternative functions.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the function of.
 *   func - The function to set the GPIO pin to use.
 *
 ****************************************************************************/

void am67_gpio_set_func(uint32_t gpio, uint32_t func);

/****************************************************************************
 * Name: am67_gpio_pin_set
 *
 * Description:
 *   Set the output of a GPIO output pin to high or low.
 *   Calling this function on a GPIO pin set as an input does nothing.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set high or low.
 *   high  - True to set the pin high, false to set the pin low.
 *
 ****************************************************************************/

void am67_gpio_pin_set(uint32_t gpio, bool set);

/****************************************************************************
 * Name: am67_gpio_pin_get
 *
 * Description:
 *   Get the current value of the GPIO.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set high or low.
 *
 * Return:
 *    True for high, false for low.
 *
 ****************************************************************************/

bool am67_gpio_pin_get(uint32_t gpio);

/****************************************************************************
 * Name: am67_gpio_event_get
 *
 * Description:
 *   Check if an event was detected for the given GPIO pin.
 *   The event bit will be set if an event has happened that matches the
 *   event detection configuration for the given pin (rising edge,
 *   falling edge, level).
 *
 * Input parameters:
 *   gpio - The GPIO pin number to check for an event.
 *
 * Return:
 *    True if an event was detected, false otherwise.
 *
 ****************************************************************************/

bool am67_gpio_event_get(uint32_t gpio);

/****************************************************************************
 * Name: am67_gpio_event_clear
 *
 * Description:
 *   Clear the event detect status for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to clear the event status of.
 *
 ****************************************************************************/

void am67_gpio_event_clear(uint32_t gpio);

/****************************************************************************
 * Name: am67_gpio_rising_edge
 *
 * Description:
 *   Set/clear rising edge event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void am67_gpio_rising_edge(uint32_t gpio, bool set);

/****************************************************************************
 * Name: am67_gpio_falling_edge
 *
 * Description:
 *   Set/clear falling edge event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void am67_gpio_falling_edge(uint32_t gpio, bool set);

/****************************************************************************
 * Name: am67_gpio_high_level
 *
 * Description:
 *   Set/clear high level event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void am67_gpio_high_level(uint32_t gpio, bool set);

/****************************************************************************
 * Name: am67_gpio_low_level
 *
 * Description:
 *   Set/clear low level event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void am67_gpio_low_level(uint32_t gpio, bool set);

/****************************************************************************
 * Name: am67_gpio_rising_edge_async
 *
 * Description:
 *   Set/clear async rising edge event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void am67_gpio_rising_edge_async(uint32_t gpio, bool set);

/****************************************************************************
 * Name: am67_gpio_falling_edge_async
 *
 * Description:
 *   Set/clear async falling edge event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void am67_gpio_falling_edge_async(uint32_t gpio, bool set);

/****************************************************************************
 * Name: am67_gpio_irq_attach
 *
 * Description:
 *   Attach an interrupt handler for the specified GPIO pin.
 *   NOTE: Interrupt mode (rising edge, falling edge, etc.) is configured
 *   separately.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to attach the handler for.
 *   isr - The interrupt handler function.
 *   arg - The argument to be passed to the interrupt handler.
 *
 ****************************************************************************/

int am67_gpio_irq_attach(uint32_t gpio, xcpt_t isr, void *arg);

/****************************************************************************
 * Name: am67_gpio_irq_detach
 *
 * Description:
 *   Detach an interrupt handler for a GPIO pin. NOTE: this does not disable
 *   interrupts for that particular pin; this must be done by disabling event
 *   detection for that pin separately.
 *   This function just detaches the pin's ISR, ensuring it won't be called
 *   when an interrupt is triggered.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to detach the handler of.
 *
 ****************************************************************************/

void am67_gpio_irq_detach(uint32_t gpio);


/**
 *  \brief  This API configures the direction of a specified GPIO pin as being
 *          either input or output
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= #GPIO_MAX_PIN_PER_INSTANCE - 1)
 *  \param  pinDir      The required direction for the GPIO pin.
 *          This can take one of the value from the following enum:
 *          - \ref GPIO_Direction_t.
 */
void GPIO_setDirMode(uint32_t baseAddr, uint32_t pinNum, uint32_t pinDir);

/**
 *  \brief  This API drives an output GPIO pin to a logic HIGH state.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= #GPIO_MAX_PIN_PER_INSTANCE - 1)
 */
void GPIO_pinWriteHigh(uint32_t baseAddr, uint32_t pinNum);

/**
 *  \brief  This API drives an output GPIO pin to a logic LOW state.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= #GPIO_MAX_PIN_PER_INSTANCE - 1)
 */
void GPIO_pinWriteLow(uint32_t baseAddr, uint32_t pinNum);

/**
 *  \brief  This API reads the logic level(value) on a specified
 *          GPIO pin.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= #GPIO_MAX_PIN_PER_INSTANCE - 1)
 *
 *  \return  \ref GPIO_PinValue_t
 */
uint32_t GPIO_pinRead(uint32_t baseAddr, uint32_t pinNum);

/**
 *  \brief  This API determines the output logic level(value) on a specified
 *          GPIO pin.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= #GPIO_MAX_PIN_PER_INSTANCE - 1)
 *
 *  \return \ref GPIO_PinValue_t
 */
uint32_t GPIO_pinOutValueRead(uint32_t baseAddr, uint32_t pinNum);

/**
 *  \brief  This API configures the trigger type for a specified input GPIO pin.
 *
 *          Whenever the selected trigger occurs on that GPIO pin and if interrupt
 *          generation is enabled for that pin, the GPIO module will send an
 *          interrupt to CPU.
 *          To disable the per pin event/interrupt generation, set the
 *          trigger type to #GPIO_TRIG_TYPE_NONE
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= 63).
 *  \param  trigType    This specifies the trigger type on whose detection,
 *                      the GPIO module will send an interrupt to CPU,
 *                      provided interrupt generation for that pin is enabled.
 *          'trigType' can take one of the following values following enum:
 *          - \ref GPIO_TrigType_t.
 */
void GPIO_setTrigType(uint32_t baseAddr, uint32_t pinNum, uint32_t trigType);

/**
 *  \brief  This API enables the bank interrupt. This has to be called after
 *          setting all the GPIO pin triggers of a bank to get interrupt
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  bankNum     The bank index
 *          'bankNum' can take one of the following values:
 *          (0 <= bankNum <= #GPIO_MAX_BANKS - 1)
 */
void GPIO_bankIntrEnable(uint32_t baseAddr, uint32_t bankNum);

/**
 *  \brief  This API disables the bank interrupt
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  bankNum     The bank index
 *          'bankNum' can take one of the following values:
 *          (0 <= bankNum <= #GPIO_MAX_BANKS - 1)
 */
void GPIO_bankIntrDisable(uint32_t baseAddr, uint32_t bankNum);

/**
 *  \brief  This API determines the enabled interrupt status of a specified pin.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= #GPIO_MAX_PIN_PER_INSTANCE - 1)
 *
 *  \return intrStatus The enabled interrupt status of the pin on the specified
 *                     interrupt line.
 *                     1 - interrupt occurred
 *                     0 - interrupt hasnt occurred since last cleared
 *                         writing 1 clears the corresponding interrupt status
 */
//inline uint32_t GPIO_getIntrStatus(uint32_t baseAddr, uint32_t pinNum);

/**
 *  \brief  This API clears the enabled interrupt status of a specified GPIO
 *          pin.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  pinNum      The number of the pin in the GPIO instance
 *          'pinNum' can take one of the following values:
 *          (0 <= pinNum <= #GPIO_MAX_PIN_PER_INSTANCE - 1)
 */
//inline void GPIO_clearIntrStatus(uint32_t baseAddr, uint32_t pinNum);

/**
 *  \brief  This API returns the interrupt status of the specified bank.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  bankNum     Bank index of the GPIO instance.
 *
 *  \return Raw interrupt status for the provided register index
 *
 */
//inline uint32_t GPIO_getBankIntrStatus(uint32_t baseAddr, uint32_t bankNum);

/**
 *  \brief  This API clears the interrupt status of the specified bank.
 *
 *  \param  baseAddr    The memory address of the GPIO instance being used
 *  \param  bankNum     Bank index of the GPIO instance.
 *  \param  intrStatus  Interrupt mask to clear the status
 */
//inline void GPIO_clearBankIntrStatus(uint32_t baseAddr,
                                            //uint32_t bankNum,
                                          //  uint32_t intrStatus);

#endif /* __ARCH_ARM_SRC_AM67_AM67_GPIO_H */
