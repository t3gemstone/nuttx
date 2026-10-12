/****************************************************************************
 * arch/arm/src/am67/am67_rti.c
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

/* RTI8, the digital windowed watchdog (DWWD) of MAIN R5FSS0 core 0.
 *
 * The expiry reaction is an NMI on VIM input 30, routed as FIQ.  FIQs stay
 * enabled while a task holds IRQs off (up_irq_save() only sets CPSR.I), so
 * a core stuck with interrupts disabled still gets here.  The handler cuts
 * every motor output, records where the core was for Linux to read, and
 * halts.
 *
 * Once enabled the DWD cannot be disabled or reprogrammed until the module
 * is reset.  A firmware restarted by remoteproc may therefore find it
 * running with the previous image's preload; am67_rti_wdt_start() then
 * keeps that timeout.  The window is 100 %, so a kick is never early.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/cache.h>
#include <nuttx/irq.h>

#include "arm_internal.h"
#include "am67_rti.h"
#include "am67_tisci.h"

#if defined(CONFIG_AM67_EPWM0) || defined(CONFIG_AM67_EPWM1)
#  include "am67_pwm.h"
#endif
#if defined(CONFIG_AM67_ECAP0) || defined(CONFIG_AM67_ECAP1) || \
    defined(CONFIG_AM67_ECAP2)
#  include "am67_ecap.h"
#endif

#ifdef CONFIG_AM67_RTI8_WDT

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RTI8_BASE               0x0e0a0000u

#define RTI_DWDCTRL             0x90u   /* enable key */
#define RTI_DWDPRLD             0x94u   /* 12-bit preload */
#define RTI_WDSTATUS            0x98u   /* W1C violation flags */
#define RTI_WDKEY               0x9cu   /* kick sequence */
#define RTI_DWDCNTR             0xa0u   /* down counter */
#define RTI_DWWDRXNCTRL         0xa4u   /* reaction */
#define RTI_DWWDSIZECTRL        0xa8u   /* window size */

#define RTI_DWD_ENABLE          0xa98559dau
#define RTI_RXN_NMI             0x0000000au
#define RTI_WINDOW_100          0x00000005u
#define RTI_WDKEY1              0x0000e51au
#define RTI_WDKEY2              0x0000a35cu
#define RTI_WDSTATUS_ALL        0x0000003eu

/* CTRL_MMR0_CFG0_WWD8_CLKSEL: 0 = HFOSC0 (25 MHz on this board) */

#define WWD8_CLKSEL             0x001083a0u
#define WWD8_CLKSEL_MASK        0x3u
#define RTI_FCLK_HZ             25000000u

/* Expiry time = (preload + 1) * 2^13 / FCLK */

#define RTI_PRLD_SHIFT          13
#define RTI_PRLD_MAX            0xfffu

/* VIM input 30: RTI8 intr_wwd (pulse) */

#define AM67_RTI8_IRQ           30
#define VIM_BASE                0x2fff0000u
#define VIM_STS(j)              (VIM_BASE + 0x404u + (((j) >> 5) * 0x20u))
#define VIM_INT_MAP(j)          (VIM_BASE + 0x418u + (((j) >> 5) * 0x20u))
#define VIM_INT_TYPE(j)         (VIM_BASE + 0x41cu + (((j) >> 5) * 0x20u))
#define VIM_FIQVEC              (VIM_BASE + 0x1cu)
#define VIM_ACTFIQ              (VIM_BASE + 0x24u)
#define VIM_BIT(j)              (1u << ((j) & 0x1fu))

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Where the core was when the watchdog fired, for post-mortem reading from
 * Linux (scripts/kart/r5f-inspect.sh read32): magic, pc, lr, cpsr, sp and
 * the VIM input taken.  Cleaned to DDR after every update, since a halted
 * core never evicts the cache line.
 */

#ifdef CONFIG_AM67_RESET_ON_FATAL
volatile uint32_t g_am67_wdt_fiq_info[AM67_WDT_INFO_WORDS]
  aligned_data(32) locate_data(".am67_noinit");
#else
volatile uint32_t g_am67_wdt_fiq_info[AM67_WDT_INFO_WORDS]
  aligned_data(32);
#endif

void am67_wdt_info_set(int index, uint32_t value)
{
  g_am67_wdt_fiq_info[index] = value;
  up_clean_dcache((uintptr_t)g_am67_wdt_fiq_info,
                  (uintptr_t)g_am67_wdt_fiq_info +
                  sizeof(g_am67_wdt_fiq_info));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int am67_rti_wdt_start(uint32_t timeout_ms)
{
  uint32_t prld;

#ifdef CONFIG_AM67_RESET_ON_FATAL
  /* The record survives the SoC reset that followed the expiry */

  if (g_am67_wdt_fiq_info[0] == AM67_WDT_INFO_MAGIC)
    {
      syslog(LOG_ERR, "[wdt] previous run ended in a watchdog expiry: "
             "pc 0x%08" PRIx32 " lr 0x%08" PRIx32 " cpsr 0x%08" PRIx32
             " sp 0x%08" PRIx32 " vim 0x%08" PRIx32 "\n",
             g_am67_wdt_fiq_info[1], g_am67_wdt_fiq_info[2],
             g_am67_wdt_fiq_info[3], g_am67_wdt_fiq_info[4],
             g_am67_wdt_fiq_info[5]);
    }

  am67_wdt_info_set(0, 0);
#endif

  if ((getreg32(WWD8_CLKSEL) & WWD8_CLKSEL_MASK) != 0)
    {
      return -ENODEV;   /* not HFOSC0: the timeout below would be wrong */
    }

  if (getreg32(RTI8_BASE + RTI_DWDCTRL) != RTI_DWD_ENABLE)
    {
      prld = (uint32_t)(((uint64_t)timeout_ms * RTI_FCLK_HZ / 1000u) >>
                        RTI_PRLD_SHIFT);
      prld = prld > 0 ? prld - 1 : 0;
      prld = prld > RTI_PRLD_MAX ? RTI_PRLD_MAX : prld;

      putreg32(prld, RTI8_BASE + RTI_DWDPRLD);
      putreg32(RTI_RXN_NMI, RTI8_BASE + RTI_DWWDRXNCTRL);
      putreg32(RTI_WINDOW_100, RTI8_BASE + RTI_DWWDSIZECTRL);
      putreg32(RTI_DWD_ENABLE, RTI8_BASE + RTI_DWDCTRL);
    }

  am67_rti_wdt_kick();
  putreg32(RTI_WDSTATUS_ALL, RTI8_BASE + RTI_WDSTATUS);

  prld = getreg32(RTI8_BASE + RTI_DWDPRLD) & RTI_PRLD_MAX;
  return (int)((((uint64_t)prld + 1) << RTI_PRLD_SHIFT) * 1000u /
               RTI_FCLK_HZ);
}

void am67_rti_wdt_kick(void)
{
  putreg32(RTI_WDKEY1, RTI8_BASE + RTI_WDKEY);
  putreg32(RTI_WDKEY2, RTI8_BASE + RTI_WDKEY);
}

uint32_t am67_rti_wdt_counter(void)
{
  return getreg32(RTI8_BASE + RTI_DWDCNTR);
}

uint32_t am67_rti_wdt_status(void)
{
  return getreg32(RTI8_BASE + RTI_WDSTATUS);
}

void am67_rti_wdt_arm_fiq(void)
{
  irqstate_t flags = up_irq_save();

  /* Drop an expiry latched while nobody was kicking (for example during
   * the boot after a remoteproc restart), so it is not taken as a fault.
   */

  putreg32(RTI_WDSTATUS_ALL, RTI8_BASE + RTI_WDSTATUS);
  putreg32(VIM_BIT(AM67_RTI8_IRQ), VIM_STS(AM67_RTI8_IRQ));

  modifyreg32(VIM_INT_TYPE(AM67_RTI8_IRQ), 0, VIM_BIT(AM67_RTI8_IRQ));
  modifyreg32(VIM_INT_MAP(AM67_RTI8_IRQ), 0, VIM_BIT(AM67_RTI8_IRQ));
  up_enable_irq(AM67_RTI8_IRQ);

  up_irq_restore(flags);
}

/****************************************************************************
 * Name: arm_decodefiq
 *
 * Description:
 *   The watchdog is the only FIQ that reaches this handler (the DShot
 *   engine takes its own FIQ before it, am67_dshot_fiq.S), so any FIQ here
 *   is fatal.  Cut the motors, record where the core was and which VIM
 *   input fired, then never return: whatever hung the control loop is still
 *   there.  Linux can read g_am67_wdt_fiq_info.
 *
 ****************************************************************************/

uint32_t *arm_decodefiq(uint32_t *regs)
{
  uint32_t act;
  int irq;

#if defined(CONFIG_AM67_EPWM0) || defined(CONFIG_AM67_EPWM1)
  am67_epwm_emergency_stop();
#endif
#if defined(CONFIG_AM67_ECAP0) || defined(CONFIG_AM67_ECAP1) || \
    defined(CONFIG_AM67_ECAP2)
  am67_ecap_emergency_stop();
#endif

  /* Take the FIQ at the VIM, clear its status, and end it by writing
   * FIQVEC, as arm_decodeirq() does with IRQVEC.
   */

  (void)getreg32(VIM_FIQVEC);
  act = getreg32(VIM_ACTFIQ);
  irq = (act & 0x80000000u) != 0 ? (int)(act & 0x3ffu) : AM67_RTI8_IRQ;
  putreg32(VIM_BIT(irq), VIM_STS(irq));
  putreg32(RTI_WDSTATUS_ALL, RTI8_BASE + RTI_WDSTATUS);
  putreg32(irq, VIM_FIQVEC);

  am67_wdt_info_set(1, regs[REG_PC]);
  am67_wdt_info_set(2, regs[REG_LR]);
  am67_wdt_info_set(3, regs[REG_CPSR]);
  am67_wdt_info_set(4, regs[REG_SP]);
  am67_wdt_info_set(5, act);
  am67_wdt_info_set(0, AM67_WDT_INFO_MAGIC);

#ifdef CONFIG_AM67_RESET_ON_FATAL
  /* Restart through U-Boot without waiting for anyone: the log and the
   * record above must reach DDR first.
   */

  up_flush_dcache_all();
  (void)am67_tisci_sys_reset_now();
#endif

  /* Stay here with every interrupt source disabled at the VIM.  The core
   * does not answer the remoteproc mailbox any more: `echo stop` times out
   * (-EBUSY) and Linux keeps a consistent state; recovering the core needs
   * a Linux reboot.  (Parking in WFI after a hang never held: WFI returned
   * at once on every cycle with no IRQ, FIQ or abort pending, so a stop
   * acknowledged from here failed Linux's WFI check.  See REHBER 5.23.)
   */

  for (irq = 0; irq < NR_IRQS; irq++)
    {
      up_disable_irq(irq);
    }

  for (; ; )
    {
      asm volatile ("wfi");
    }
}

#endif /* CONFIG_AM67_RTI8_WDT */
