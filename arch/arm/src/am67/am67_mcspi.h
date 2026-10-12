/****************************************************************************
 * arch/arm/src/am67/am67_mcspi.h
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_MCSPI_H
#define __ARCH_ARM_SRC_AM67_AM67_MCSPI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

#include <nuttx/spi/spi.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MCU_MCSPI0 on Gemstone O1 (MCU domain) */

#define AM67_MCSPI0_BASE           0x04b00000u
#define AM67_MCSPI0_IRQ            207

/* K3 MCU_MCSPI0 register map (TRM offsets).  The peripheral prepends an
 * HL header block (HL_REV/HL_HWINFO/HL_SYSCONFIG) at 0x000-0x01f; the
 * functional registers therefore start at 0x100, not 0x000 as in the
 * older OMAP2 map.
 */

/* HL header registers (0x000-0x01f) */

#define AM67_MCSPI_HL_SYSCONFIG    0x010

/* HL_SYSCONFIG bits */

#define AM67_MCSPI_HL_SYSCONFIG_NOIDLE             (1u << 2)

/* Functional registers start at 0x100 */

#define AM67_MCSPI_REVISION        0x100
#define AM67_MCSPI_SYSCONFIG       0x110
#define AM67_MCSPI_SYSSTATUS       0x114
#define AM67_MCSPI_IRQSTATUS       0x118
#define AM67_MCSPI_IRQENABLE       0x11c
#define AM67_MCSPI_WAKEUPENABLE    0x120
#define AM67_MCSPI_SYST            0x124
#define AM67_MCSPI_MODULCTRL       0x128
#define AM67_MCSPI_CHCONF0         0x12c
#define AM67_MCSPI_CHSTAT0         0x130
#define AM67_MCSPI_CHCTRL0         0x134
#define AM67_MCSPI_TX0             0x138
#define AM67_MCSPI_RX0             0x13c
#define AM67_MCSPI_XFERLEVEL       0x17c

#define AM67_MCSPI_NCHANNELS       4u
#define AM67_MCSPI_CH_OFFSET(n)    ((uint32_t)(n) * 0x14u)

#define AM67_MCSPI_SYSCONFIG_AUTOIDLE           (1u << 0)
#define AM67_MCSPI_SYSCONFIG_SOFTRESET          (1u << 1)
#define AM67_MCSPI_SYSCONFIG_SIDLEMODE_SMART    (2u << 3)
#define AM67_MCSPI_SYSCONFIG_SIDLEMODE_NO       (1u << 3)
#define AM67_MCSPI_SYSCONFIG_CLKACT_BOTH        (3u << 8)
#define AM67_MCSPI_SYSSTATUS_RESETDONE          (1u << 0)

#define AM67_MCSPI_MODULCTRL_SINGLE (1u << 0)
#define AM67_MCSPI_MODULCTRL_MS     (1u << 2)

#define AM67_MCSPI_CHCONF_PHA       (1u << 0)
#define AM67_MCSPI_CHCONF_POL       (1u << 1)
#define AM67_MCSPI_CHCONF_CLKD_SHIFT 2
#define AM67_MCSPI_CHCONF_CLKD_MASK (0x0fu << AM67_MCSPI_CHCONF_CLKD_SHIFT)
#define AM67_MCSPI_CHCONF_EPOL      (1u << 6)
#define AM67_MCSPI_CHCONF_WL_SHIFT  7
#define AM67_MCSPI_CHCONF_WL_MASK   (0x1fu << AM67_MCSPI_CHCONF_WL_SHIFT)
#define AM67_MCSPI_CHCONF_TRM_RX    (1u << 12)
#define AM67_MCSPI_CHCONF_TRM_TX    (1u << 13)
#define AM67_MCSPI_CHCONF_DPE0      (1u << 16)
#define AM67_MCSPI_CHCONF_DPE1      (1u << 17)
#define AM67_MCSPI_CHCONF_IS        (1u << 18)
#define AM67_MCSPI_CHCONF_TURBO     (1u << 19)
#define AM67_MCSPI_CHCONF_FORCE           (1u << 20)
#define AM67_MCSPI_CHCONF_SPIENSLV_SHIFT  21
#define AM67_MCSPI_CHCONF_SPIENSLV_MASK   (3u << AM67_MCSPI_CHCONF_SPIENSLV_SHIFT)
#define AM67_MCSPI_CHCONF_FFEW            (1u << 27)
#define AM67_MCSPI_CHCONF_FFER            (1u << 28)
#define AM67_MCSPI_CHCONF_CLKG            (1u << 29)

#define AM67_MCSPI_CHSTAT_RXS       (1u << 0)
#define AM67_MCSPI_CHSTAT_TXS       (1u << 1)
#define AM67_MCSPI_CHSTAT_EOT       (1u << 2)
#define AM67_MCSPI_CHSTAT_TXFFE     (1u << 3)
#define AM67_MCSPI_CHSTAT_TXFFF     (1u << 4)
#define AM67_MCSPI_CHSTAT_RXFFE     (1u << 5)
#define AM67_MCSPI_CHSTAT_RXFFF     (1u << 6)

/* IRQSTATUS/IRQENABLE: per channel TXi_EMPTY, TXi_UNDERFLOW, RXi_FULL at
 * bits 4i, 4i+1, 4i+2; end of word count (FIFO transfers) at bit 17.
 */

#define AM67_MCSPI_IRQ_RX_FULL(ch)  (1u << (4u * (ch) + 2u))
#define AM67_MCSPI_IRQ_EOW          (1u << 17)
#define AM67_MCSPI_IRQ_ALL          0x0003ffffu

/* XFERLEVEL: word count and almost-empty/almost-full levels */

#define AM67_MCSPI_XFERLEVEL_WCNT_SHIFT 16
#define AM67_MCSPI_XFERLEVEL_WCNT_MAX   0xffffu
#define AM67_MCSPI_XFERLEVEL_AFL_SHIFT  8

/* The 64-byte FIFO is split in two 32-byte halves when both directions
 * use it.
 */

#define AM67_MCSPI_FIFO_HALF        32u

#define AM67_MCSPI_CHCTRL_EN        (1u << 0)
#define AM67_MCSPI_CHCTRL_EXTCLK_SHIFT 8
#define AM67_MCSPI_CHCTRL_EXTCLK_MASK  (0xffu << AM67_MCSPI_CHCTRL_EXTCLK_SHIFT)

/* Functional clock (MCU_PLL0 path); the divider is applied in the driver */

#ifndef CONFIG_AM67_MCSPI0_FCLK
#  define CONFIG_AM67_MCSPI0_FCLK  48000000
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void am67_spiinitialize(void);
FAR struct spi_dev_s *am67_spibus_initialize(int port);
void am67_mcspi_board_select(FAR struct spi_dev_s *dev, uint8_t channel,
                             bool selected);

/* Number of failed words and status waits since the last call, then reset
 * to 0. SPI exchange() cannot return an error, so a caller that must not
 * use a failed transfer reads this after deselecting the device.
 */

int am67_mcspi_take_errors(FAR struct spi_dev_s *dev);

/* Time spent in the driver, in R5F cycles (PMU cycle counter). For
 * diagnostics; the fields are updated under the bus lock and read without
 * it, so one sample can be torn.
 */

struct am67_mcspi_stats_s
{
  uint32_t transfers;        /* select/deselect pairs */
  uint32_t words;            /* words shifted by exchange()/send() */
  uint64_t xfer_cycles;      /* exchange()/send() */
  uint64_t select_cycles;    /* select + deselect, incl. the EOT wait */
  uint64_t config_cycles;    /* setfrequency/setmode/setbits */
  uint32_t max_xfer_cycles;  /* longest single exchange() */
  uint32_t max_xfer_words;   /* its length */
  uint32_t fifo_stalls;      /* FIFO transfers with no progress */
  uint32_t eot_timeouts;     /* EOT not seen after a FIFO transfer */
  uint32_t fail_stat;        /* CHSTAT at the last failure */
  uint32_t fail_rx;          /* words received before it */
  uint32_t irq_xfers;        /* FIFO transfers completed by the interrupt */
  uint32_t irq_timeouts;     /* interrupt-driven transfers that timed out */
};

void am67_mcspi_stats(FAR struct spi_dev_s *dev,
                      FAR struct am67_mcspi_stats_s *stats, bool reset);

#ifdef CONFIG_AM67_FAULT_INJECTION
/* Make the next count FIFO transfers stall (no words sent, or the
 * interrupt never enabled), so that they time out. Bench tests only.
 */

void am67_mcspi_inject_fault(unsigned int count);
#endif

#endif /* __ARCH_ARM_SRC_AM67_AM67_MCSPI_H */
