/****************************************************************************
 * arch/arm/src/am67/am67_irq.c
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

#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <arch/irq.h>

#include "arm_internal.h"
#include "am67_irq.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void am67_error_handler(void)
{
  PANIC();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize

 * The device supports three different possibilities for software to handle
 * interrupts:
 *
 *   1. Index interrupts mode (compatible with TMS470R1x legacy code),
 *   2. Register vectored interrupts (automatically provide vector address
 *      to application)
 *   3. Hardware vectored interrupts (automatically dispatch to ISR, IRQ
 *      only)
 *
 * Only the indexed mode is supported here: After the interrupt is received
 * by the CPU, the CPU branches to 0x18 (IRQ) or 0x1C (FIQ) to execute the
 * main ISR. The main ISR routine reads the offset register (IRQINDEX,
 * FIQINDEX) to determine the source of the interrupt.
 *
 * To use mode 2), it would only be necessary to initialize the VIM_RAM.
 * To use mode 3), it would be necessary to initialize the VIM_RAM and also
 * to set the vector enable (VE) bit in the CP15 R1 register.  This bit is
 * zero on reset so that the default state after reset is backward
 * compatible to earlier ARM CPU.
 *
 ****************************************************************************/


void up_irqinitialize(void)
{
}

/****************************************************************************
 * Name: arm_decodeirq
 *
 * Description:
 *   This function is called from the IRQ vector handler in arm_vectors.S.
 *   At this point, the interrupt has been taken and the registers have
 *   been saved on the stack.  This function simply needs to determine the
 *   the irq number of the interrupt and then to call arm_doirq to dispatch
 *   the interrupt.
 *
 * Input Parameters:
 *   regs - A pointer to the register save area on the stack.
 *
 ****************************************************************************/

uint32_t *arm_decodeirq(uint32_t *regs)
{
  return regs;
}

/****************************************************************************
 * Name: arm_decodefiq
 *
 * Description:
 *   This function is called from the FIQ vector handler in arm_vectors.S.
 *   At this point, the interrupt has been taken and the registers have
 *   been saved on the stack.  This function simply needs to determine the
 *   the irq number of the interrupt and then to call arm_doirq to dispatch
 *   the interrupt.
 *
 *  Input Parameters:
 *   regs - A pointer to the register save area on the stack.
 *
 ****************************************************************************/

#ifdef CONFIG_ARMV7R_HAVE_DECODEFIQ
uint32_t *arm_decodefiq(uint32_t *regs)
{
  return regs;
}
#endif

/****************************************************************************
 * Name: up_disable_irq
 *
 * Description:
 *   Disable the IRQ or FIQ specified by 'channel'
 *
 ****************************************************************************/

void up_disable_irq(int channel)
{
}

/****************************************************************************
 * Name: up_enable_irq
 *
 * Description:
 *   Enable the IRQ specified by 'channel'
 *
 ****************************************************************************/

void up_enable_irq(int channel)
{
}

/****************************************************************************
 * Name: up_enable_fiq
 *
 * Description:
 *   Enable the FIQ specified by 'channel'
 *
 ****************************************************************************/

#ifdef CONFIG_ARMV7R_HAVE_DECODEFIQ
void up_enable_fiq(int channel)
{
}
#endif

/****************************************************************************
 * Name: arm_ack_irq
 *
 * Description:
 *   Acknowledge the IRQ
 *
 ****************************************************************************/

void arm_ack_irq(int irq)
{
}
