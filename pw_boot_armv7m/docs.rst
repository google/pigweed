.. _chapter-pw-boot-armv7m:

.. default-domain:: cpp

.. highlight:: sh

--------------
pw_boot_armv7m
--------------

The ARMv7-M boot module provides a linker script and some early initialization
of static memory regions and C++ constructors. This is enough to get many
ARMv7-M cores booted and ready to run C++ code.

This module is currently designed to support a very minimal device memory layout
configuration:

 - One contiguous region for RAM.
 - One contiguous region for flash.
 - Static, in-flash vector table at the default location expected by the SoC.

Note that this module is not yet particularly suited for projects that utilize
a bootloader, as it's relatively opinionated regarding where code is stored.

.. warning::
  This module is currently NOT stable! Depending on this module may cause
  breakages as this module is updated.

Setup
=====

User-Implemented Functions
--------------------------
This module expects two extern "C" functions to be defined outside this module.

 - ``int main()``: This is where applications reside.
 - ``void pw_PreStaticConstructorInit()``: This function executes just before
   C++ static constructors are called. At this point, other static memory has
   been zero or data initialized. This function should set up any early
   initialization that should be done before C++ static constructors are run
   (e.g. enabling FPU).
 - ``void pw_PreMainInit()``: This function executes just before main, and
   can be used for any device initialization that isn't application specific.
   Depending on your platform, this might be turning on a UART, setting up
   default clocks, etc.

If either of these functions are unimplemented, executables will encounter a
link error.

Required Configs
----------------
This module has a number of required configuration options that mold the linker
script to fit to a wide variety of ARMv7-M SoCs. The ``pw_boot_armv7m_config``
GN variable has a ``defines`` member that can be used to modify these linker
script options. See the documentation section on configuration for information
regarding which configuration options are required.

Vector Table
------------
Targets using ``pw_boot_armv7m`` will need to provide an ARMv7-M interrupt
vector table (ARMv7-M Architecture Reference Manual DDI 0403E.b section B1.5.2
and B1.5.3). This is done by storing an array into the ``.vector_table``
section, and properly configuring ``PW_BOOT_VECTOR_TABLE_*`` preprocessor
defines to cover the address region your SoC expects the vector table to be
located at (often the beginning of the flash region). If using a bootloader,
ensure VTOR (Vector Table Offset Register) is configured to point to the vector
table. Otherwise, refer to the hardware vendor's documentation to determine
where the vector table should be located such that it resides where VTOR is
initialized to by default.

Example vector table:

.. code-block:: cpp

  typedef void (*InterruptHandler)();

  PW_KEEP_IN_SECTION(".vector_table")
  const InterruptHandler vector_table[] = {
      // The starting location of the stack pointer.
      // This address is NOT an interrupt handler/function pointer, it is simply
      // the address that the main stack pointer should be initialized to. The
      // value is reinterpret casted because it needs to be in the vector table.
      [0] = reinterpret_cast<InterruptHandler>(&pw_stack_high_addr),

      // Reset handler, dictates how to handle reset interrupt. This is the
      // address that the Program Counter (PC) is initialized to at boot.
      [1] = pw_BootEntry,

      // NMI handler.
      [2] = DefaultFaultHandler,
      // HardFault handler.
      [3] = DefaultFaultHandler,
      ...
  };

Usage
=====

Publicly exported symbols
-------------------------
The linker script provided by this module exports a number of symbols that
may be used to retrieve the locations of specific memory regions at runtime.
These symbols are declared as ``uint8_t`` variables. The variables themselves
do not contain the addresses, they only reside at the memory location they
reference. To retrieve the memory locations, simply take the reference of the
symbol (e.g. ``&pw_vector_table_addr``).

``pw_heap_[low/high]_addr``: Beginning and end of the memory range of the heap.
These addresses may be identical, indicating a heap with a size of zero bytes.

``pw_stack_[low/high]_addr``: Beginning and end of the memory range of the main
stack. This might not be the only stack in the system.

``pw_vector_table_addr``: Beginning of the ARMv7-M interrupt vector table.

Configuration
=============
These configuration options can be controlled by appending list items to
``pw_boot_armv7m_LINK_CONFIG_DEFINES`` as part of a Pigweed target
configuration.

``PW_BOOT_HEAP_SIZE`` (required):
How much memory (in bytes) to reserve for the heap. This can be zero.

``PW_BOOT_MIN_STACK_SIZE`` (required):
The minimum size reserved for the main stack. If statically allocated memory
begins to cut into the minimum, a link error will be emitted.

``PW_BOOT_FLASH_BEGIN`` (required):
The start address of the MCU's flash region. This region must NOT include the
vector table. (i.e. if the VECTOR_TABLE is in flash, the flash region
should begin *after* the vtable)

``PW_BOOT_FLASH_SIZE`` (required):
Size of the flash region in bytes.

``PW_BOOT_RAM_BEGIN`` (required):
The start address of the MCU's RAM region.

``PW_BOOT_RAM_SIZE`` (required):
Size of the RAM region in bytes.

``PW_BOOT_VECTOR_TABLE_BEGIN`` (required):
Address the target MCU expects the link-time vector table to be located at. This
is typically the beginning of the flash region. While the vector table may be
changed later in the boot process, a minimal vector table MUST be present for
the MCU to operate as expected.

``PW_BOOT_VECTOR_TABLE_SIZE`` (required):
Number of bytes to reserve for the ARMv7-M vector table.

Dependencies
============
  * ``pw_preprocessor`` module
