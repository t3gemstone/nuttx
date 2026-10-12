/****************************************************************************
 * arch/arm/src/am67/am67_dshot_fiq.S
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

/* DShot bit engine, entered straight from the FIQ vector (am67_dshot.c).
 * Runs in FIQ mode on the banked r8-r12 only: no stack, no saved context.
 * r8 = &g_am67_dshot (struct am67_dshot_fiq_s), set once at install.
 * r9 = VIM base from entry up to the STS write after label 7: nothing on
 * the way there may reuse it, or the FIQVEC read and the STS write land in
 * ATCM (g_am67_dshot + 0x424 is IRQ code) and the next IRQ crashes.
 * Anything but the EPWM0 event trigger (VIM 36) continues at the previous
 * vector target, the generic NuttX FIQ entry, with lr/spsr untouched.
 *
 * Each FIQ writes one row: the compares of both modules (shadowed, loaded
 * at the next CTR = 0) and their continuous software force (AQCSFRC,
 * loaded at CTR = PRD, one tick before that).  A pin is held low by the
 * force, never by a zero compare: a compare loaded at CTR = 0 does not act
 * at that same CTR = 0, so 0 -> n loses the first pulse and n -> 0 gives
 * one full high period.  The force loads at PRD, where no action is
 * programmed, so it never races an edge.
 *
 * Two timing faults drop the frame (the force goes low from the next PRD
 * on and the ESC sees a short frame, which it rejects):
 *  - late: the next CTR = 0 (or the PRD before it) passed while the row
 *    was being written, so the channels may disagree about the bit;
 *  - skipped: the FIQ came more than a period after its CTR = 0.  The
 *    event flag was still set at the next CTR = 0, so that one raised no
 *    FIQ and the previous bit went out twice.  The bit 0 FIQ anchors the
 *    frame (PMU cycles and TBCNT at its exit); every later FIQ checks how
 *    many periods have passed since.
 * Row 0 only synchronises (the first FIQ can come at any point of a
 * period) and holds the pins low, so it is exempt from both checks.
 *
 * Row: [0] EPWM0 CMPA | CMPB << 16, [1] EPWM1 CMPA | CMPB << 16,
 *      [2] EPWM0 AQCSFRC | EPWM1 AQCSFRC << 16.
 */

#include <nuttx/config.h>

#ifdef CONFIG_AM67_DSHOT

	.syntax	unified
	.arm

	.section .am67_fastcode, "ax"
	.globl	am67_dshot_fiq
	.type	am67_dshot_fiq, %function

am67_dshot_fiq:
	ldr	r9, =0x2fff0000			/* VIM */
	ldr	r10, [r9, #0x0c]		/* PRIFIQ: pending FIQ, not taken */
	ldr	r11, =0x800003ff		/* VALID | NUM */
	and	r10, r10, r11
	ldr	r11, =0x80000024		/* VALID, input 36 */
	cmp	r10, r11
	ldrne	pc, [r8, #20]			/* not ours: generic FIQ entry */

	mrc	p15, 0, r11, c9, c13, 0		/* PMCCNTR at entry */
	str	r11, [r8, #40]
	ldr	r10, [r8, #116]			/* gap since the previous entry */
	str	r11, [r8, #116]
	ldr	r12, [r8, #0]
	cmp	r12, #2				/* rows 2.. of a frame only */
	blo	7f
	sub	r10, r11, r10
	ldr	r12, [r8, #112]
	cmp	r10, r12
	strhi	r10, [r8, #112]			/* worst gap, cycles */
	strhi	lr, [r8, #120]
	ldr	r12, [r8, #68]			/* gap over two periods (cycles */
	add	r12, r12, r12, lsl #2		/* > 2 * 3.2 * TBPRD = 6.4): log */
	lsl	r12, r12, #1
	add	r12, r12, r12, lsr #2
	cmp	r10, r12
	bls	7f
	ldr	r12, [r8, #124]			/* ring index */
	and	r11, r12, #15			/* not r9: it holds the VIM base */
	add	r11, r8, r11, lsl #3		/* for label 7 (FIQVEC, STS) */
	str	r10, [r11, #128]		/* gap, cycles */
	str	lr, [r11, #132]			/* lr_fiq */
	add	r12, r12, #1
	str	r12, [r8, #124]
7:
	ldr	r10, [r9, #0x1c]		/* FIQVEC read: take it */
	ldr	r10, [r8, #12]			/* EPWM0 */
	mov	r11, #1
	strh	r11, [r10, #0x38]		/* ETCLR.INT */
	mov	r11, #(1 << 4)
	str	r11, [r9, #0x424]		/* VIM STS: input 36 = group 1 bit 4 */

	ldr	r11, [r8, #0]			/* row index */
	ldr	r12, [r8, #8]			/* row table, 12 bytes per row */
	add	r12, r12, r11, lsl #3
	add	r12, r12, r11, lsl #2
	add	r11, r11, #1
	str	r11, [r8, #0]

	ldr	r11, [r12, #0]			/* EPWM0 compares */
	strh	r11, [r10, #0x12]
	lsr	r11, r11, #16
	strh	r11, [r10, #0x14]
	ldr	r11, [r12, #8]			/* EPWM0 force */
	strh	r11, [r10, #0x1c]
	ldr	r10, [r8, #16]			/* EPWM1 */
	ldr	r11, [r12, #4]
	strh	r11, [r10, #0x12]
	lsr	r11, r11, #16
	strh	r11, [r10, #0x14]
	ldr	r11, [r12, #8]
	lsr	r11, r11, #16
	strh	r11, [r10, #0x1c]

	/* Late check: the row must have landed before the next PRD.  TBCNT
	 * at exit counts TBCLK ticks since the CTR = 0 that raised this FIQ,
	 * unless another one passed meanwhile; then it is smaller than the
	 * ticks spent in here (PMU cycles * 250 MHz / 800 MHz).  The cycle
	 * counter is read before TBCNT, so without a wrap TBCNT >= spent
	 * holds whatever the read latency; 2 ticks cover the quantisation of
	 * the two unrelated clocks.  TBCNT = TBPRD means the force may have
	 * missed its load.
	 */

	dsb					/* row written */
	mrc	p15, 0, r12, c9, c13, 0
	str	r12, [r8, #72]			/* PMCCNTR at exit */
	ldr	r10, [r8, #12]
	ldrh	r11, [r10, #0x08]		/* TBCNT at exit */
	ldr	r9, [r8, #40]
	sub	r12, r12, r9
	str	r12, [r8, #88]			/* cycles in the handler */
	ldr	r9, [r8, #0]
	cmp	r9, #1
	beq	2f				/* sync row: no check */
	cmp	r9, #2
	bne	4f

	ldr	r9, [r8, #40]			/* bit 0: anchor the frame */
	str	r9, [r8, #48]
	ldr	r9, [r8, #72]
	str	r9, [r8, #76]			/* PMCCNTR at exit */
	str	r11, [r8, #80]			/* TBCNT at exit */
	ldr	r9, [r8, #68]
	add	r9, r9, #1
	lsr	r9, r9, #1
	str	r9, [r8, #84]			/* limit: half a period */
	b	5f

4:
	ldr	r9, [r8, #84]			/* limit += one period */
	ldr	r10, [r8, #68]
	add	r9, r9, r10
	add	r9, r9, #1
	str	r9, [r8, #84]
	ldr	r10, [r8, #72]			/* ticks since the anchor */
	ldr	r12, [r8, #76]
	sub	r10, r10, r12
	add	r10, r10, r10, lsl #2
	lsr	r10, r10, #4
	ldr	r12, [r8, #80]			/* + anchor TBCNT - TBCNT: ticks */
	add	r10, r10, r12			/* from the anchor's CTR = 0 to */
	sub	r10, r10, r11			/* this FIQ's CTR = 0 */
	cmp	r10, r9
	bhs	6f				/* one more period: skipped */

5:
	ldr	r12, [r8, #88]
	ldr	r9, [r8, #44]
	cmp	r12, r9
	strhi	r12, [r8, #44]			/* worst handler cycles */
	ldr	r9, [r8, #36]
	cmp	r11, r9
	strhi	r11, [r8, #36]			/* worst exit count */
	strhi	lr, [r8, #96]			/* and where the core was */
	ldr	r9, [r8, #68]
	cmp	r11, r9, lsr #1
	ldrhi	r9, [r8, #100]
	addhi	r9, r9, #1
	strhi	r9, [r8, #100]			/* rows past half a period */
	add	r12, r12, r12, lsl #2
	lsr	r12, r12, #4			/* cycles -> TBCLK ticks */
	add	r9, r11, #2
	cmp	r9, r12
	blo	3f				/* CTR = 0 passed: late */
	ldr	r9, [r8, #68]
	cmp	r11, r9
	bhs	3f				/* at PRD: late */

2:
	ldr	r11, [r8, #0]
	ldr	r12, [r8, #4]			/* rows per frame */
	cmp	r11, r12
	blo	1f

	ldr	r11, [r8, #40]			/* last row: bit 0 to here */
	ldr	r12, [r8, #48]
	sub	r11, r11, r12
	str	r11, [r8, #52]
	ldr	r10, [r8, #12]
	ldrh	r11, [r10, #0x32]		/* stop */
	bic	r11, r11, #8			/* ETSEL.INTEN off */
	strh	r11, [r10, #0x32]
	mov	r11, #0
	str	r11, [r8, #24]			/* busy = 0 */
	ldr	r11, [r8, #28]
	add	r11, r11, #1
	str	r11, [r8, #28]			/* frames++ */
	b	1f

6:
	str	lr, [r8, #104]			/* where the core was */
	ldr	r12, [r8, #92]
	add	r12, r12, #1
	str	r12, [r8, #92]			/* skipped++, then drop as late */
	mov	r12, r10

3:
	ldr	r10, [r8, #12]
	ldr	r9, [r8, #0]			/* diagnostics: row, exit, spent */
	str	r9, [r8, #56]
	str	r11, [r8, #60]
	str	r12, [r8, #64]
	mov	r11, #5				/* drop the frame: both pins low */
	strh	r11, [r10, #0x1c]		/* from the next PRD on */
	ldr	r12, [r8, #16]
	strh	r11, [r12, #0x1c]
	ldrh	r11, [r10, #0x32]
	bic	r11, r11, #8			/* ETSEL.INTEN off */
	strh	r11, [r10, #0x32]
	mov	r11, #0
	str	r11, [r8, #24]			/* busy = 0 */
	ldr	r11, [r8, #32]
	add	r11, r11, #1
	str	r11, [r8, #32]			/* late++ */

1:
	ldr	r9, =0x2fff0000
	mov	r11, #36
	str	r11, [r9, #0x1c]		/* end the FIQ at the VIM */
	dsb
	mrc	p15, 0, r11, c9, c13, 0
	ldr	r12, [r8, #40]
	sub	r11, r11, r12
	ldr	r12, [r8, #108]
	cmp	r11, r12
	strhi	r11, [r8, #108]			/* worst whole FIQ, cycles */
	subs	pc, lr, #4

	.ltorg
	.size	am67_dshot_fiq, . - am67_dshot_fiq

#endif /* CONFIG_AM67_DSHOT */
	.end
