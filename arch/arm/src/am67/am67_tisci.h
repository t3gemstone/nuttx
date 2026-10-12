/****************************************************************************
 * arch/arm/src/am67/am67_tisci.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_AM67_AM67_TISCI_H
#define __ARCH_ARM_SRC_AM67_AM67_TISCI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* J722S TISCI device IDs of what this core drives (Linux DTS k3_pds) */

#define AM67_TISCI_DEV_TIMER(n)     (36u + (n))  /* MAIN DMTimer 0..2 */
#define AM67_TISCI_DEV_EPWM0        86u
#define AM67_TISCI_DEV_EPWM1        87u
#define AM67_TISCI_DEV_ECAP1        52u
#define AM67_TISCI_DEV_MMCSD0       57u
#define AM67_TISCI_DEV_MAIN_GPIO1   78u
#define AM67_TISCI_DEV_MCU_GPIO0    79u
#define AM67_TISCI_DEV_MCU_I2C0     106u
#define AM67_TISCI_DEV_MCU_MCSPI0   147u
#define AM67_TISCI_DEV_MAIN_UART1   152u
#define AM67_TISCI_DEV_MAIN_UART6   158u

/* DMTimer functional clock (the <&k3_clks dev 2> of the Linux DTS) */

#define AM67_TISCI_CLK_TIMER_FCK    2u

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int am67_tisci_device_on(uint32_t id);
int am67_tisci_device_require(uint32_t id);
int am67_tisci_get_device(uint32_t id, uint8_t *programmed,
                          uint8_t *current);
int am67_tisci_get_freq(uint32_t dev, uint8_t clk, uint64_t *hz);
int am67_tisci_sys_reset(void);
int am67_tisci_sys_reset_now(void);

#endif /* __ARCH_ARM_SRC_AM67_AM67_TISCI_H */
