.. _module-pw_cpu_exception:

----------------
pw_cpu_exception
----------------
Pigweed's exception module provides a consistent interface for entering an
application's CPU exception handler. While the actual exception handling
behavior is left to an application to implement, this module deals with any
architecture-specific actions required before calling the application exception
handler. More specifically, the exception module collects CPU state that may
otherwise be clobbered by an application's exception handler.

Setup
=====
An application using this module **must** connect ``pw_CpuExceptionEntry()`` to
the platform's CPU exception handler interrupt so ``pw_CpuExceptionEntry()`` is
called immediately upon a CPU exception. For specifics on how this may be done,
see the backend documentation for your architecture.

Applications must also provide an implementation for
``pw_CpuExceptionDefaultHandler()``. The behavior of this functions is entirely
up to the application/project, but some examples are provided below:

  * Enter an infinite loop so the device can be debugged by JTAG.
  * Reset the device.
  * Attempt to handle the exception so execution can continue.
  * Capture and record additional device state and save to flash for a crash
    report.
  * A combination of the above, using logic that fits the needs of your project.

Module Usage
============
Basic usage of this module entails applications supplying a definition for
``pw_CpuExceptionDefaultHandler()``. ``pw_CpuExceptionDefaultHandler()`` should
contain any logic to determine if a exception can be recovered from, as well as
necessary actions to properly recover. If the device cannot recover from the
exception, the function should **not** return.

``pw_CpuExceptionDefaultHandler()`` is called indirectly, and may be overridden
at runtime via ``pw_CpuExceptionSetHandler()``. The handler can also be reset to
point to ``pw_CpuExceptionDefaultHandler()`` by calling
``pw_CpuExceptionRestoreDefaultHandler()``.

When writing an exception handler, prefer to use the functions provided by this
interface rather than relying on the backend implementation of
``pw_CpuExceptionState``. This allows better code portability as it helps
prevent an application fault handler from being tied to a single backend.

For example; when logging or dumping CPU state, prefer ``ToString()`` or
``RawFaultingCpuState()`` over directly accessing members of a
``pw_CpuExceptionState`` object.

Some exception handling behavior may require architecture-specific CPU state to
attempt to correct a fault. In this situation, the application's exception
handler will be tied to the backend implementation of the CPU exception module.

Backend Expectations
====================
CPU exception backends do not provide an exception handler, but instead provide
mechanisms to capture CPU state for use by an application's exception handler,
and allow recovery from CPU exceptions when possible.

  * A backend should provide a definition for the ``pw_CpuExceptionState``
    struct that provides suitable means to access and modify any captured CPU
    state.
  * If an application's exception handler modifies the captured CPU state, the
    state should be treated as though it were the original state of the CPU when
    the exception occurred. The backend may need to manually restore some of the
    modified state to ensure this on exception handler return.
  * A backend should implement the ``pw_CpuExceptionEntry()`` function that will
    call ``pw_HandleCpuException()`` after performing any necessary
    actions prior to handing control to the application's exception handler
    (e.g. capturing necessary CPU state).

Compatibility
=============
Most of the pw_cpu_exception module is C-compatible. The exception to this is
the "support" facade and library, which requires C++.

Dependencies
============
  * ``pw_span``
  * ``pw_preprocessor``
