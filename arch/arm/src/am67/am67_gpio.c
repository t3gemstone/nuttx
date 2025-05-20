/****************************************************************************
 * arch/arm64/src/am67/am67_gpio.c
 *
 * Author: Emre CECANPUNAR <emreleno@gmail.com>
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

#include <nuttx/arch.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "arm_internal.h"
#include "am67_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

void GPIO_pinWriteHigh(uint32_t baseAddr, uint32_t pinNum)
{
  uint32_t regIndex, regVal;
  volatile CSL_GpioRegs* hGpio = (volatile CSL_GpioRegs*)((uintptr_t) baseAddr);

  regIndex = GPIO_GET_REG_INDEX(pinNum);
  regVal   = GPIO_GET_BIT_MASK(pinNum);
  hGpio->BANK_REGISTERS[regIndex].SET_DATA = baseAddr + 0x18; 
  //CSL_REG32_WR(&hGpio->BANK_REGISTERS[regIndex].SET_DATA, regVal);
  hGpio->BANK_REGISTERS[regIndex].SET_DATA = regVal;
  return;
}

void GPIO_pinWriteLow(uint32_t baseAddr, uint32_t pinNum)
{
    uint32_t                regIndex, regVal;
    volatile CSL_GpioRegs*  hGpio = (volatile CSL_GpioRegs*)((uintptr_t) baseAddr);

    regIndex = GPIO_GET_REG_INDEX(pinNum);
    regVal = GPIO_GET_BIT_MASK(pinNum);
    hGpio->BANK_REGISTERS[regIndex].CLR_DATA = baseAddr + 0x1C; 
    //CSL_REG32_WR(&hGpio->BANK_REGISTERS[regIndex].CLR_DATA, regVal);
    hGpio->BANK_REGISTERS[regIndex].CLR_DATA = regVal;

    return;
}

void GPIO_setDirMode(uint32_t baseAddr, uint32_t pinNum, uint32_t pinDir)
{
    uint32_t                regIndex, bitPos;
    volatile CSL_GpioRegs*  hGpio = (volatile CSL_GpioRegs*)((uintptr_t) baseAddr);

    regIndex = GPIO_GET_REG_INDEX(pinNum);
    bitPos = GPIO_GET_BIT_POS(pinNum);
    hGpio->BANK_REGISTERS[regIndex].DIR = baseAddr + 0x10; 
    CSL_FINSR(hGpio->BANK_REGISTERS[regIndex].DIR, bitPos, bitPos, (((uint32_t) pinDir) & 0x01U));

    return;
}
/*static inline uint32_t gpio_get_bank_bit(uint32_t gpio)
{
  // Position within the 32-bit register (0-15 for Bank0, 16-31 for Bank1)
  uint32_t pin_in_reg = gpio % PINS_PER_REG;
  return pin_in_reg;  // 0-31, maps directly to bit position
}

static inline uint32_t gpio_get_bank_number(uint32_t gpio)
{
  // Bank0: GPIO 0-15, Bank1: GPIO 16-31, Bank2: GPIO 32-47, etc.
  return gpio / PINS_PER_BANK;
}

static inline uint32_t gpio_get_bitmask(uint32_t gpio)
{
  uint32_t bank_bit = gpio_get_bank_bit(gpio);  // 0-31
  DEBUGASSERT(bank_bit < 32);
  return (1U << bank_bit);
}
*/
/*
 * Returns base address of that GPIO.
 * Should add sub-register offset before using such as DIR, OUT_DATA, IN_DATA etc.
 */
/*static inline uint32_t *gpio_get_register_addr(uint32_t gpio)
{
  uint32_t reg_number = gpio_get_register_number(gpio);
  return (uint32_t *)(AM67_GPIO_BASE + (reg_number * AM67_BANK_OFFSET));
}

static inline uint32_t gpio_get_register_number(uint32_t gpio)
{
  // Register 0: GPIO 0-31 (Bank0+Bank1), Register 1: GPIO 32-63 (Bank2+Bank3)
  return gpio / PINS_PER_REG;
}
*/
/****************************************************************************
 * Name: am67_gpio_help_set
 *
 * Description:
 *   Helper function for setting a GPIO pin value on the am67 GPIO
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the value of.
 *   val - The value to set (true for 1, false for 0).
 *
 ****************************************************************************/

/*static inline void am67_gpio_help_set(uint32_t gpio, bool val)
{
  DEBUGASSERT(gpio < AM67_GPIO_NUM);
  
  uint32_t bitmask = gpio_get_bitmask(gpio);
  bitmask = 1 << 11;
  uint32_t* base_addr = gpio_get_register_addr(gpio);
  uint32_t offset = val ? AM67_GPIO_SET_DATA_OFFSET : AM67_GPIO_CLR_DATA_OFFSET;
  volatile uint32_t* reg = (volatile uint32_t*)((uintptr_t)base_addr + offset);
  
  modreg32(1, bitmask, (uint32_t)reg);
}*/

/****************************************************************************
 * Name: am67_gpio_help_get
 *
 * Description:
 *   Helper function for reading a GPIO pin value on the am67 GPIO
 *
 * Input parameters:
 *   gpio - The GPIO pin number to get the value of.
 *
 ****************************************************************************/

/*static inline bool am67_gpio_help_get(uint32_t gpio)
{
  DEBUGASSERT(gpio < AM67_GPIO_NUM);
  
  uint32_t bitmask = gpio_get_bitmask(gpio);
  uint32_t reg = (uint32_t)gpio_get_register_addr(gpio);
  reg += AM67_GPIO_IN_DATA_OFFSET;

  return getreg32(reg) & bitmask;
}*/

/****************************************************************************
 * Public Functions
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

void am67_gpio_set_func(uint32_t gpio, uint32_t func)
{
  DEBUGASSERT(gpio < AM67_GPIO_NUM);
  DEBUGASSERT(func < 2);
  
  //uint32_t bitmask = gpio_get_bitmask(gpio);
  //uint32_t reg = (uint32_t)gpio_get_register_addr(gpio);
  //reg += AM67_GPIO_DIR_OFFSET;

  //modreg32(func, bitmask, reg);
}

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

void am67_gpio_pin_set(uint32_t gpio, bool set)
{
  DEBUGASSERT(gpio < AM67_GPIO_NUM);

  //am67_gpio_help_set(gpio, set);
}

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

bool am67_gpio_pin_get(uint32_t gpio)
{
  DEBUGASSERT(gpio < AM67_GPIO_NUM);

  return false;// am67_gpio_help_get(gpio);
}

