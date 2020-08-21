/*
 * Copyright 2020 The Pigweed Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

/* This relatively simplified linker script will work with many ARMv7-M cores
 * that have on-board memory-mapped RAM and FLASH. For more complex projects and
 * devices, it's possible this linker script will not be sufficient as-is.
 *
 * This linker script is likely not suitable for a project with a bootloader.
 */

/* Provide useful error messages when required configurations are not set. */
#ifndef PW_BOOT_VECTOR_TABLE_BEGIN
#error "PW_BOOT_VECTOR_TABLE_BEGIN is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_VECTOR_TABLE_BEGIN

#ifndef PW_BOOT_VECTOR_TABLE_SIZE
#error "PW_BOOT_VECTOR_TABLE_SIZE is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_VECTOR_TABLE_SIZE

#ifndef PW_BOOT_FLASH_BEGIN
#error "PW_BOOT_FLASH_BEGIN is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_FLASH_BEGIN

#ifndef PW_BOOT_FLASH_SIZE
#error "PW_BOOT_FLASH_SIZE is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_FLASH_SIZE

#ifndef PW_BOOT_RAM_BEGIN
#error "PW_BOOT_RAM_BEGIN is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_RAM_BEGIN

#ifndef PW_BOOT_RAM_SIZE
#error "PW_BOOT_RAM_SIZE is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_RAM_SIZE

#ifndef PW_BOOT_HEAP_SIZE
#error "PW_BOOT_HEAP_SIZE is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_HEAP_SIZE

#ifndef PW_BOOT_MIN_STACK_SIZE
#error "PW_BOOT_MIN_STACK_SIZE is not defined, and is required to use pw_boot_armv7m"
#endif  // PW_BOOT_MIN_STACK_SIZE


/* Note: This technically doesn't set the firmware's entry point. Setting the
 *       firmware entry point is done by setting vector_table[1]
 *       (Reset_Handler). However, this DOES tell the compiler how to optimize
 *       when --gc-sections is enabled.
 */
ENTRY(pw_boot_Entry)

MEMORY
{
  /* TODO(pwbug/57): Make it possible for projects to freely customize
   * memory regions.
   */

  /* Vector Table (typically in flash) */
  VECTOR_TABLE(rx) : \
    ORIGIN = PW_BOOT_VECTOR_TABLE_BEGIN, \
    LENGTH = PW_BOOT_VECTOR_TABLE_SIZE
  /* Internal Flash */
  FLASH(rx) : \
    ORIGIN = PW_BOOT_FLASH_BEGIN, \
    LENGTH = PW_BOOT_FLASH_SIZE
  /* Internal SRAM */
  RAM(rwx) : \
    ORIGIN = PW_BOOT_RAM_BEGIN, \
    LENGTH = PW_BOOT_RAM_SIZE
}

SECTIONS
{
  /* This is the link-time vector table. If used, the VTOR (Vector Table Offset
   * Register) MUST point to this memory location in order to be used. This can
   * be done by ensuring this section exists at the default location of the VTOR
   * so it's used on reset, or by explicitly setting the VTOR in a bootloader
   * manually to point to &pw_boot_vector_table_addr before interrupts are enabled.
   */
  .vector_table : ALIGN(512)
  {
    pw_boot_vector_table_addr = .;
    KEEP(*(.vector_table))
  } >VECTOR_TABLE

  /* Main executable code. */
  .code : ALIGN(8)
  {
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
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);

    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);

    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
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
   * This section is zero initialized in pw_boot_Entry(). */
  .zero_init_ram : ALIGN(8)
  {
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
  } >RAM

  .heap : ALIGN(8)
  {
    pw_boot_heap_low_addr = .;
    . = . + PW_BOOT_HEAP_SIZE;
    . = ALIGN(8);
    pw_boot_heap_high_addr = .;
  } >RAM

  /* Link-time check for stack overlaps. */
  .stack (NOLOAD) : ALIGN(8)
  {
    /* Set the address that the main stack pointer should be initialized to. */
    pw_boot_stack_low_addr = .;
    HIDDEN(_stack_size = ORIGIN(RAM) + LENGTH(RAM) - .);
    /* Align the stack to a lower address to ensure it isn't out of range. */
    HIDDEN(_stack_high = (. + _stack_size) & ~0x7);
    ASSERT(_stack_high - . >= PW_BOOT_MIN_STACK_SIZE,
           "Error: Not enough RAM for desired minimum stack size.");
    . = _stack_high;
    pw_boot_stack_high_addr = .;
  } >RAM
}

/* Symbols used by core_init.c: */
/* Start of .static_init_ram in FLASH. */
_pw_static_init_flash_start = LOADADDR(.static_init_ram);

/* Region of .static_init_ram in RAM. */
_pw_static_init_ram_start = ADDR(.static_init_ram);
_pw_static_init_ram_end = _pw_static_init_ram_start + SIZEOF(.static_init_ram);

/* Region of .zero_init_ram. */
_pw_zero_init_ram_start = ADDR(.zero_init_ram);
_pw_zero_init_ram_end = _pw_zero_init_ram_start + SIZEOF(.zero_init_ram);

/* arm-none-eabi expects `end` symbol to point to start of heap for sbrk. */
PROVIDE(end = _pw_zero_init_ram_end);
