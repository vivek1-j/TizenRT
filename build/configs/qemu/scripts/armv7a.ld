/****************************************************************************
 *
 * Copyright 2024 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * build/configs/qemu/scripts/armv7a.ld
 *
 * Linker script for QEMU virt platform with Cortex-A7.
 * Adapted from NuttX dramboot.ld for proper memory layout.
 *
 * Memory layout (matching QEMU defaults and NuttX approach):
 *   FLASH: 0x00000000 - 0x07FFFFFF  (128MB, vectors + code + rodata)
 *   RAM:   0x40000000 - 0x4FFFFFFF  (256MB, data + heap + stack)
 *
 * Note: QEMU -kernel loads ELF to FLASH region (0x00000000).
 *       We use identity MMU mapping (virtual = physical).
 ****************************************************************************/

OUTPUT_ARCH(arm)
ENTRY(__start)

MEMORY
{
    ROM (rx) : ORIGIN = 0x00000000, LENGTH = 128M
    RAM (rwx): ORIGIN = 0x40000000, LENGTH = 256M
}

SECTIONS
{
    /* Text section with vectors at the beginning
     * Vectors must be first so CPU finds them at reset address 0x00000000
     */
    .text : {
        _stext = .;
        __text_start = .;
        *(.vectors)
        *(.text*)
        *(.fixup)
        *(.gnu.warning)
    } > ROM

    /* Init section - constructor/destructor tables
     * Aligned to 4KB boundary for proper MMU mapping
     */
    .init_section : {
        _sinit = ABSOLUTE(.);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array EXCLUDE_FILE(*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o) .ctors))
        _einit = ABSOLUTE(.);
        . = ALIGN(4096);
        _etext = .;
        _sztext = _etext - _stext;
    } > ROM

    /* Exception index tables for C++ unwinding */
    .ARM.extab : {
        *(.ARM.extab*)
    } > ROM

    /* ARM exception index section */
    PROVIDE_HIDDEN (__exidx_start = .);
    .ARM.exidx : {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > ROM
    PROVIDE_HIDDEN (__exidx_end = .);

    /* Read-only data section - SEPARATE from .text
     * This ensures _erodata correctly marks end of read-only data
     */
    .rodata : {
        _srodata = .;
        *(.rodata*)
        *(.data.rel.ro*)
        KEEP(*(SORT(.scattered_array*)));
        . = ALIGN(4096);
        _erodata = .;
    } > ROM

    /* CRITICAL FIX: _eronly = load address of .data in ROM/FLASH
     * This is where .data initial values are stored (not in RAM)
     * The startup code copies from _eronly to _sdata at boot
     */
    _eronly = LOADADDR(.data);

    /* Initialized data section
     * Lives in RAM at runtime, but initial values stored in ROM
     */
    .data : {
        _sdata = .;
        *(.data*)
        . = ALIGN(8);
        _edata = .;
    } > RAM AT > ROM

    /* No-init section - preserved across resets */
    .noinit : {
        _snoinit = ABSOLUTE(.);
        *(.noinit*)
        _enoinit = ABSOLUTE(.);
    } > RAM

    /* Zero-initialized data (BSS)
     * Startup code zeros this section before main()
     */
    .bss : {
        _sbss = .;
        *(.bss*)
        . = ALIGN(4096);
        _ebss = .;
    } > RAM

    /* End of statically allocated memory
     * Heap starts from this address
     */
    _end = .;

    /* TizenRT-specific symbols for compatibility */
    PROVIDE(_vectors_start = _stext);
    PROVIDE(_vectors_end = _stext + 0x100);

    /* Stabs debugging sections */
    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr) }
    .stab.excl 0 : { *(.stab.excl) }
    .stab.exclstr 0 : { *(.stab.exclstr) }
    .stab.index 0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment 0 : { *(.comment) }

    /* Discard exit sections (not needed for embedded) */
    /DISCARD/ : {
        *(.exit.text)
        *(.exit.data)
        *(.exitcall.exit)
    }
}
