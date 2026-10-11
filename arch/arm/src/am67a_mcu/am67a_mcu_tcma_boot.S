/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_tcma_boot.S
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Cortex-R5 MPU Region Sizes: 2^(N+1)
 * 32KB = 2^15 -> N=14 -> (14 << 1) | 1 = 0x1D
 * 16MB = 2^24 -> N=23 -> (23 << 1) | 1 = 0x2F
 * 2GB  = 2^31 -> N=30 -> (30 << 1) | 1 = 0x3D
 */

#define MPU_SIZE_32KB_EN    ((14 << 1) | 1)
#define MPU_SIZE_16MB_EN    ((23 << 1) | 1)
#define MPU_SIZE_2GB_EN     ((30 << 1) | 1)

/* MPU Region Attributes:
 * RACR TCMA: Full RW (AP=3), Non-cached (C=0, B=1, TEX=1), Non-shareable (S=0),
 *            Executable (XN=0) -> 0x0309
 * RACR DDR:  Full RW (AP=3), Cacheable (C=1, B=1, TEX=1), Non-shareable (S=0),
 *            Executable (XN=0) -> 0x030b
 * RACR Device: Full RW (AP=3), Non-cacheable, Execute-Never (XN=1) -> 0x1300
 */

#define MPU_ATTR_TCMA_RWX   0x0309
#define MPU_ATTR_DDR_RWX    0x030b
#define MPU_ATTR_DEVICE_XN  0x1300

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.syntax	unified
	.arm

	.globl	_start
	.globl	_vectors
	.globl	_vector_start
	.globl	am67a_mcu_tcma_reset

/****************************************************************************
 * Vector Table (Placed at 0x00000000 in TCMA)
 ****************************************************************************/

	.section	.vectors, "ax"
_vector_start:
_vectors:
_start:
	ldr		pc, .Lresethandler
	ldr		pc, .Lundefinedhandler
	ldr		pc, .Lsvchandler
	ldr		pc, .Lprefetchaborthandler
	ldr		pc, .Ldataaborthandler
	ldr		pc, .Laddrexcptnhandler
	ldr		pc, .Lirqhandler
	ldr		pc, .Lfiqhandler

	.globl	arm_vectorundefinsn
	.globl	arm_vectorsvc
	.globl	arm_vectorprefetch
	.globl	arm_vectordata
	.globl	arm_vectoraddrexcptn
	.globl	arm_vectorirq
	.globl	arm_vectorfiq

.Lresethandler:
	.long	am67a_mcu_tcma_reset
.Lundefinedhandler:
	.long	arm_vectorundefinsn
.Lsvchandler:
	.long	arm_vectorsvc
.Lprefetchaborthandler:
	.long	arm_vectorprefetch
.Ldataaborthandler:
	.long	arm_vectordata
.Laddrexcptnhandler:
	.long	arm_vectoraddrexcptn
.Lirqhandler:
	.long	arm_vectorirq
.Lfiqhandler:
	.long	arm_vectorfiq

/****************************************************************************
 * Name: am67a_mcu_tcma_reset
 *
 * Description:
 *   Reset entry point for TI AM67A_MCU MCU Domain Cortex-R5F.
 *   Executes from TCMA (0x00000000) out of reset.
 *   Programs initial MPU regions to allow DDR SDRAM code execution and
 *   then branches to NuttX __start in DDR.
 *
 ****************************************************************************/

	.section	.tcma_boot, "ax"
	.type	am67a_mcu_tcma_reset, %function

am67a_mcu_tcma_reset:
	/* 1. Ensure System mode with IRQ and FIQ disabled */

	cpsid		if, #0x1f

	/* 2. Set temporary stack pointer to top of TCMA (0x00007FF0) */

	ldr		sp, =0x00007ff0

	/* 3. Invalidate caches and branch predictor */

	mov		r0, #0
	mcr		p15, 0, r0, c7, c5, 0	/* Invalidate ICache */
	mcr		p15, 0, r0, c7, c5, 6	/* Invalidate branch predictor */
	mcr		p15, 0, r0, c15, c5, 0	/* Invalidate DCache */
	isb		sy

	/* 4. Disable MPU before programming regions */

	mrc		p15, 0, r0, c1, c0, 0	/* Read SCTLR */
	bic		r0, r0, #0x1			/* Clear MPU enable bit */
	dsb		sy
	mcr		p15, 0, r0, c1, c0, 0	/* Write SCTLR */
	isb		sy

	/* 5. Region 0: Peripherals / MMIO (0x00000000, 2GB, Device, XN) */

	mov		r0, #0					/* Region 0 */
	ldr		r1, =0x00000000			/* Base: 0x00000000 */
	mov		r2, #MPU_SIZE_2GB_EN	/* Size: 2GB enabled */
	ldr		r3, =MPU_ATTR_DEVICE_XN	/* Device XN */
	mcr		p15, 0, r0, c6, c2, 0	/* RGNR */
	mcr		p15, 0, r1, c6, c1, 0	/* RBAR */
	mcr		p15, 0, r2, c6, c1, 2	/* RSZR */
	mcr		p15, 0, r3, c6, c1, 4	/* RACR */

	/* 6. Region 1: TCMA (0x00000000, 32KB, Normal, Executable)
	 *    Higher region number (1 > 0) overrides Region 0 for TCMA space.
	 */

	mov		r0, #1					/* Region 1 */
	ldr		r1, =0x00000000			/* Base: 0x00000000 */
	mov		r2, #MPU_SIZE_32KB_EN	/* Size: 32KB enabled */
	ldr		r3, =MPU_ATTR_TCMA_RWX	/* TCMA Normal RWX */
	mcr		p15, 0, r0, c6, c2, 0	/* RGNR */
	mcr		p15, 0, r1, c6, c1, 0	/* RBAR */
	mcr		p15, 0, r2, c6, c1, 2	/* RSZR */
	mcr		p15, 0, r3, c6, c1, 4	/* RACR */

	/* 7. Region 5: DDR SDRAM (0x80000000, 2GB, Normal, Executable)
	 *    Matches Region 5 in am67a_mcu_mpuinit.c so DDR execution remains
	 *    continuously valid when am67a_mcu_mpu_init() is called later.
	 */

	mov		r0, #5					/* Region 5 */
	ldr		r1, =0x80000000			/* Base: 0x80000000 */
	mov		r2, #MPU_SIZE_2GB_EN	/* Size: 2GB enabled */
	ldr		r3, =MPU_ATTR_DDR_RWX	/* DDR Normal RWX */
	mcr		p15, 0, r0, c6, c2, 0	/* RGNR */
	mcr		p15, 0, r1, c6, c1, 0	/* RBAR */
	mcr		p15, 0, r2, c6, c1, 2	/* RSZR */
	mcr		p15, 0, r3, c6, c1, 4	/* RACR */

	/* 8. Enable MPU and Background Region in SCTLR */

	mrc		p15, 0, r0, c1, c0, 0
	orr		r0, r0, #(1 << 0)		/* SCTLR_M: MPU Enable */
	orr		r0, r0, #(1 << 17)		/* SCTLR_BR: Background Region Enable */
	bic		r0, r0, #(1 << 13)		/* SCTLR_V: Clear High Vectors (0x0) */
	dsb		sy
	mcr		p15, 0, r0, c1, c0, 0
	isb		sy

	/* 9. Branch to NuttX main entry point in DDR */

	ldr		pc, =__start
	.size	am67a_mcu_tcma_reset, . - am67a_mcu_tcma_reset
