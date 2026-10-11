/****************************************************************************
 * boards/arm/j722s/j722s-evm/scripts/sdram.ld
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

MEMORY
{
    /* TI J722S MCU R5F TCMA: 0x00000000 (32KB) */
    tcma (rx)     : ORIGIN = 0x00000000, LENGTH = 32K

    /* TI J722S MCU R5F Resource Table & Trace: 0xA1100000 (1MB, Non-cached) */
    rsc_tbl (rw)  : ORIGIN = 0xa1100000, LENGTH = 1M

    /* TI J722S MCU R5F DDR Carveout: 0xA1200000 (14MB) */
    sdram (rwx)   : ORIGIN = 0xa1200000, LENGTH = 14M
}

OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

SECTIONS
{
    .vectors : ALIGN(8) {
        _svectors = .;
        KEEP(*(.vectors))
        _evectors = .;
    } > tcma

    .tcma_boot : ALIGN(8) {
        *(.tcma_boot .tcma_boot.*)
    } > tcma

    .resource_table : ALIGN(4096) {
        _srsctable = .;
        KEEP(*(.resource_table))
        _ersctable = .;
    } > rsc_tbl

    .trace_buf : ALIGN(128) {
        *(.trace_buf)
    } > rsc_tbl

    .text : ALIGN(8) {
        _stext = .;
        *(.text .text.*)
        *(.fixup)
        *(.gnu.warning)
        *(.rodata .rodata.*)
        *(.gnu.linkonce.t.*)
        *(.glue_7)
        *(.glue_7t)
        *(.got)
        *(.gcc_except_table)
        *(.gnu.linkonce.r.*)
        *(.ARM.extab*)
        *(.gnu.linkonce.armextab.*)
        _etext = .;
    } > sdram

    .ARM.extab : {
        *(.ARM.extab*)
    } > sdram

    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > sdram

    .init_section : {
        _sinit = .;
        KEEP(*(SORT_BY_NAME(.init_array.*)))
        KEEP(*(.init_array))
        _einit = .;
    } > sdram

    /* RemoteProc runs directly from loaded RAM image: LMA == VMA */
    .data : ALIGN(8) {
        _sdata = .;
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        CONSTRUCTORS
        _edata = .;
    } > sdram

    _sidata = _sdata;

    .bss (NOLOAD) : ALIGN(8) {
        _sbss = .;
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        _ebss = .;
    } > sdram

    /* Heap memory starts immediately after BSS */
    .heap (NOLOAD) : ALIGN(8) {
        _sheap = .;
    } > sdram
}
