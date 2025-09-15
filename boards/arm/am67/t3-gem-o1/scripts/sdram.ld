
MEMORY
{
  ATCM    (rwx) : ORIGIN = 0x00000000, LENGTH = 32K   /* code + vectors */
  BTCM    (rwx) : ORIGIN = 0x41010000, LENGTH = 32K   /* data/bss/stacks */
  RSC     (rwx) : ORIGIN = 0xA2100000, LENGTH = 1K  /* Main SDRAM for code and data */
  MSRAM   (rwx) : ORIGIN = 0xA2200000, LENGTH = 0xE00000 
}
EXTERN(_vector_start)
ENTRY(_vector_start)

SECTIONS
{
    .resource_table :
    {
        KEEP(*(.resource_table))
        . = ALIGN(1024);
    } > RSC 

    .vectors :
    {
        
        _stext = ABSOLUTE(.);
        KEEP(*(.vectors))
        KEEP(*arm_head*.o(.text))
        KEEP(*(.text.arm_boot))
        KEEP(*(.text.am67_mpu_initialize))
        KEEP(*(.text.mpu_control))
        KEEP(*(.text.cp15_rdsctlr))
        KEEP(*(.text.am67_mpu_reset))
        KEEP(*(.text.mpu_set_region_zero))
        KEEP(*(.text.am67_mpu_disableBR))
        KEEP(*(.text.mpu_configure_region))
        KEEP(*(.text.mpu_allocregion))
        KEEP(*(.text.mpu_modify_region))
        KEEP(*(.text.mpu_set_rgnr))
        KEEP(*(.text.mpu_set_drbar))
        KEEP(*(.text.mpu_log2regionceil))
        KEEP(*(.text.mpu_subregion))
        KEEP(*(.text.mpu_subregion_ms))
        KEEP(*(.text.mpu_subregion_ls))
        KEEP(*(.text.mpu_set_drsr))
        KEEP(*(.text.mpu_set_dracr))
        KEEP(*(.text.cp15_wrsctlr))
        KEEP(*(.text.nx_start))
        KEEP(*(.tickTimer))
        KEEP(*(.text.nxsched_process_timer))
        KEEP(*(.text.clock_timer))
        KEEP(*(.text.wd_timer))
        KEEP(*(.text.clock_systime_ticks))
    } > ATCM

    .text_ddr :
    {   
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
        _etext = ABSOLUTE(.);
    } > MSRAM

    .init_section :
    {
        _sinit = ABSOLUTE(.);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array EXCLUDE_FILE(*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o) .ctors))
        _einit = ABSOLUTE(.);
    } > MSRAM

    .ARM.extab :
    {
        *(.ARM.extab*)
    } > MSRAM

    /* .ARM.exidx is sorted, so has to go in its own output section.  */
    PROVIDE_HIDDEN (__exidx_start = .);
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > MSRAM 
    PROVIDE_HIDDEN (__exidx_end = .);

    .data :
    {
        _sdata = ABSOLUTE(.);
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        CONSTRUCTORS
        . = ALIGN(4);
        _edata = ABSOLUTE(.);
    } > MSRAM 

    /* Uninitialized data */
    .noinit :
    {
        _snoinit = ABSOLUTE(.);
        *(.noinit*)
        _enoinit = ABSOLUTE(.);
    } > MSRAM 

    .bss :
    {
        _sbss = ABSOLUTE(.);
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = ABSOLUTE(.);
        
    } > MSRAM 

    /* Stabs debugging sections. */
    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr) }
    .stab.excl 0 : { *(.stab.excl) }
    .stab.exclstr 0 : { *(.stab.exclstr) }
    .stab.index 0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment 0 : { *(.comment) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_info 0 : { *(.debug_info) }
    .debug_line 0 : { *(.debug_line) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_aranges 0 : { *(.debug_aranges) }
}

