/**
 * Copyright 2019 The Pigweed Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

HIDDEN(_min_stack_size = 1K);

/* Note: This technically doesn't set the firmware's entry point. Setting the
 *       firmware entry point is done by setting vector_table[1] in core_init.c.
 *       However, this DOES tell the compiler how to optimize when --gc-sections
 *       is enabled.
 */
ENTRY(pw_FirmwareInit)

MEMORY
{
  /* Internal Flash */
  FLASH(rx) : ORIGIN = 0x08000000, LENGTH = 512K
  /* Internal SRAM */
  RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 192K
}

SECTIONS
{

  /* Main executable code. */
  .code : ALIGN(8)
  {
    /* STM32F4xx expects the vector table to be at the beginning of flash. */
    KEEP(*(.vector_table))

    . = ALIGN(8);
    /* Application code. */
    *(.text)
    *(.text*)
    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(8);
    /* Constants.*/
    *(.rodata)
    *(.rodata*)

    /* .preinit_array, .init_array, .fini_array are used by libc.
     * Each section is a list of function pointers that are called pre-main and
     * post-exit for object initialization and tear-down.
     * Since the region isn't explicitly referenced, specify KEEP to prevent
     * link-time garbage collection. SORT is used for sections that have strict
     * init/de-init ordering requirements. */
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);

    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);

    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* Used by unwind-arm/ */
  .ARM : ALIGN(8) {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  /* Explicitly initialized global and static data. (.data)*/
  .static_init_ram : ALIGN(8)
  {
    *(.data)
    *(.data*)
    . = ALIGN(8);
  } >RAM AT> FLASH

  /* Zero initialized global/static data. (.bss)
   * This section is zero initialized in pw_FirmwareInit(). */
  .zero_init_ram : ALIGN(8)
  {
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
  } >RAM

  /* Link-time check for stack overlaps. */
  .stack (NOLOAD) : ALIGN(8)
  {
    HIDDEN(_stack_size = ORIGIN(RAM) + LENGTH(RAM) - .);
    ASSERT(_stack_size >= _min_stack_size, "Error: Not enough RAM for stack.");
    . = . + _stack_size;
  } >RAM
}

/* Symbols used by core_init.c: */
/* Top of stack to set stack pointer. */
_stack_end = ORIGIN(RAM) + LENGTH(RAM);

/* Start of .static_init_ram in FLASH. */
_static_init_flash_start = LOADADDR(.static_init_ram);

/* Region of .static_init_ram in RAM. */
_static_init_ram_start = ADDR(.static_init_ram);
_static_init_ram_end = _static_init_ram_start + SIZEOF(.static_init_ram);

/* Region of .zero_init_ram. */
_zero_init_ram_start = ADDR(.zero_init_ram);
_zero_init_ram_end = _zero_init_ram_start + SIZEOF(.zero_init_ram);

/* arm-none-eabi expects `end` symbol to point to start of heap for sbrk. */
PROVIDE (end = _zero_init_ram_end);
