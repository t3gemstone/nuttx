/****************************************************************************
 * arch/arm/src/am67/am67_pinmux.c
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

#include <sys/types.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "arm_internal.h"
#include "am67_pinmux.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static struct pinmux_conf gpinmux_conf[] = 
{
    // USART0 pin config

    // UART0_RXD -> UART0_RXD (F19)
    {
        PIN_UART0_RXD,
        ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },

    // UART0_TXD -> UART0_TXD (F20)
    {
        PIN_UART0_TXD,
        ( PIN_MODE(0) | PIN_PULL_DISABLE )
    },

    {PINMUX_END, PINMUX_END}
};

/****************************************************************************
 *  pinmux_config configures the padconfig register using the given
 *  pinmux_conf. Only MAIN domain is considered in the implementation,
 *  however, I guess it is quite easy to add MCU by only changing the base 
 *  address of the related padconfig register, which is defined in am67_pinmux.h
 *  but not used here. The only difficulty would be address translation related
 *  things, which is not implemented.
 *****************************************************************************/

void pinmux_unlock(void)    // Unlock to be able to modify pinmux settings
{
    uint32_t base_addr;
    volatile uint32_t *kick_addr;

    base_addr = CSL_PADCFG_CTRL0_CFG0_BASE;

    // Lock 0
    kick_addr = (volatile uint32_t *)(base_addr + CSL_MAIN_PADCONFIG_LOCK0_KICK0_OFFSET);
    CSL_REG32_WR(kick_addr, KICK0_UNLOCK_VAL);
    kick_addr++;
    CSL_REG32_WR(kick_addr, KICK1_UNLOCK_VAL);

    // Lock 1
    kick_addr = (volatile uint32_t *)(base_addr + CSL_MAIN_PADCONFIG_LOCK1_KICK0_OFFSET);
    CSL_REG32_WR(kick_addr, KICK0_UNLOCK_VAL);
    kick_addr++;
    CSL_REG32_WR(kick_addr, KICK1_UNLOCK_VAL);
}

void pinmux_lock(void)
{
    // I guess some SoCs do not need that,
    // implement later if needed.
}

void pinmux_config(const struct pinmux_conf *pinmux_conf)
{
    uint32_t base_addr;
    volatile uint32_t *reg_addr;

    if (pinmux_conf != NULL)
    {
        base_addr = CSL_PADCFG_CTRL0_CFG0_BASE + PADCFG_PMUX_OFFSET;

        // There is an address translation here actually, add if needed later.

        pinmux_unlock();

        while (pinmux_conf->offset != PINMUX_END)     // Set all the configuration fields
        {
            reg_addr = (volatile uint32_t *)(base_addr + pinmux_conf->offset);
            CSL_REG32_WR(reg_addr, pinmux_conf->setting);
            pinmux_conf++;
        }

        pinmux_lock();
    }
}


void pinmux_init(void)
{
    pinmux_config(gpinmux_conf);
}

