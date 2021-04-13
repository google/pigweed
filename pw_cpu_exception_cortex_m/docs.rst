.. _module-pw_cpu_exception_cortex_m:

-------------------------
pw_cpu_exception_cortex_m
-------------------------
This backend provides an ARMv7-M implementation for the CPU exception module
frontend. See the CPU exception frontend module description for more
information.

Setup
=====
There are a few ways to set up the ARMv7-M exception handler so the
application's exception handler is properly called during an exception.

**1. Use existing CMSIS functions**
  Inside of CMSIS fault handler functions, branch to ``pw_cpu_exception_Entry``.

  .. code-block:: cpp

    __attribute__((naked)) void HardFault_Handler(void) {
    asm volatile(
        " ldr r0, =pw_cpu_exception_Entry  \n"
        " bx r0                            \n");
    }

**2. Modify a startup file**
  Assembly startup files for some microcontrollers initialize the interrupt
  vector table. The functions to call for fault handlers can be changed here.
  For ARMv7-M, the fault handlers are indexes 3 to 6 of the interrupt vector
  table. It's also may be helpful to redirect the NMI handler to the entry
  function (if it's otherwise unused in your project).

  Default:

  .. code-block:: cpp

    __isr_vector_table:
      .word  __stack_start
      .word  Reset_Handler
      .word  NMI_Handler
      .word  HardFault_Handler
      .word  MemManage_Handler
      .word  BusFault_Handler
      .word  UsageFault_Handler

  Using CPU exception module:

  .. code-block:: cpp

    __isr_vector_table:
      .word  __stack_start
      .word  Reset_Handler
      .word  pw_cpu_exception_Entry
      .word  pw_cpu_exception_Entry
      .word  pw_cpu_exception_Entry
      .word  pw_cpu_exception_Entry
      .word  pw_cpu_exception_Entry

  Note: ``__isr_vector_table`` and ``__stack_start`` are example names, and may
  vary by platform. See your platform's assembly startup script.

**3. Modify interrupt vector table at runtime**
  Some applications may choose to modify their interrupt vector tables at
  runtime. The ARMv7-M exception handler works with this use case (see the
  exception_entry_test integration test), but keep in mind that your
  application's exception handler will not be entered if an exception occurs
  before the vector table entries are updated to point to
  ``pw_cpu_exception_Entry``.

Module Usage
============
For lightweight exception handlers that don't need to access
architecture-specific registers, using the generic exception handler functions
is preferred.

However, some projects may need to explicitly access architecture-specific
registers to attempt to recover from a CPU exception. ``pw_cpu_exception_State``
provides access to the captured CPU state at the time of the fault. When the
application-provided ``pw_cpu_exception_DefaultHandler()`` function returns, the
CPU state is restored. This allows the exception handler to modify the captured
state so that execution can safely continue.

Expected Behavior
-----------------
In most cases, the CPU state captured by the exception handler will contain the
ARMv7-M basic register frame in addition to an extended set of registers (see
``cpu_state.h``). The exception to this is when the program stack pointer is in
an MPU-protected or otherwise invalid memory region when the CPU attempts to
push the exception register frame to it. In this situation, the PC, LR, and PSR
registers will NOT be captured and will be marked with 0xFFFFFFFF to indicate
they are invalid. This backend will still be able to capture all the other
registers though.

In the situation where the main stack pointer is in a memory protected or
otherwise invalid region and fails to push CPU context, behavior is undefined.

Nested Exceptions
-----------------
To enable nested fault handling:
  1. Enable separate detection of usage/bus/memory faults via the SHCSR.
  2. Decrease the priority of the memory, bus, and usage fault handlers. This
     gives headroom for escalation.

While this allows some faults to nest, it doesn't guarantee all will properly
nest.

Configuration Options
=====================

 - ``PW_CPU_EXCEPTION_EXTENDED_CFSR_DUMP``: Enable extended logging in
   ``pw::cpu_exception::LogCpuState()`` that dumps the active CFSR fields with
   help strings. This is disabled by default since it increases the binary size
   by >1.5KB when using plain-text logs, or ~460 Bytes when using tokenized
   logging. It's useful to enable this for device bringup until your application
   has an end-to-end crash reporting solution.

Exception Analysis
==================
This module provides Python tooling to analyze CPU state captured by a Cortex-M
core during an exception. This can be useful as part of a crash report analyzer.

CFSR decoder
------------
The ARMv7-M and ARMv8-M architectures have a Configurable Fault Status Register
(CFSR) that explains what illegal behavior caused a fault. This module provides
a simple command-line tool to decode CFSR contents (e.g. 0x00010000) as
human-readable information (e.g. "Encountered invalid instruction").

For example:

  .. code-block::

    $ python -m pw_cpu_exception_cortex_m.cfsr_decoder 0x00010100
    20210412 15:11:14 INF Exception caused by a usage fault, bus fault.

    Active Crash Fault Status Register (CFSR) fields:
    IBUSERR     Bus fault on instruction fetch.
    UNDEFINSTR  Encountered invalid instruction.

    All registers:
    cfsr       0x00010100

.. note::
  The CFSR is not supported on ARMv6-M CPUs (Cortex M0, M0+, M1).
