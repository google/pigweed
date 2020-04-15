.. _chapter-pw-assert:

.. default-domain:: cpp

.. highlight:: cpp

=========
pw_assert
=========

--------
Overview
--------
Pigweed's assert module provides facilities for applications to check
preconditions. The module is split into two components:

1. The facade (this module) which is only a macro interface layer
2. The backend, provided elsewhere, that implements the low level checks

All behaviour is controlled by the backend.

The ``pw_assert`` public API provides three classes of macros:

+-----------------------------------------+--------------------------------+
| PW_CRASH(format, ...)                   | Trigger a crash with a message |
+-----------------------------------------+--------------------------------+
| PW_CHECK(condition[, format, ...])      | Assert a condition, optionally |
|                                         | with a message                 |
+-----------------------------------------+--------------------------------+
| PW_CHECK_<type>_<cmp>(a, b[, fmt, ...]) | Assert that the expression     |
|                                         | ``a <cmp> b`` is true,         |
|                                         | optionally with a message.     |
+-----------------------------------------+--------------------------------+

.. tip::
  All of the assert macros optionally support a message with additional
  arguments, to assist in debugging when an assert triggers:

  .. code-block:: cpp

    PW_CHECK_INT_LE(ItemCount(), 100);
    PW_CHECK_INT_LE(ItemCount(), 100, "System state: %s", GetStateStr());

Compatibility
-------------
The facade is compatible with C and C++.

Example
-------

.. code-block:: cpp

  #include "pw_assert/assert.h"

  int main() {
    bool sensor_running = StartSensor(&msg);
    PW_CHECK(sensor_running, "Sensor failed to start; code: %s", msg);

    int temperature_c = ReadSensorCelcius();
    PW_CHECK_INT_LE(temperature_c, 100,
                    "System is way out of heat spec; state=%s",
                    ReadSensorStateString());
  }


---------------------------
Assert facade API reference
---------------------------

The below functions describe the assert API functions that applications should
invoke to assert.

.. cpp:function:: PW_CRASH(format, ...)

  Trigger a crash with a message. Replaces LOG_FATAL() in other systems. Can
  include a message with format arguments; for example:

  .. code-block:: cpp

    PW_CRASH("Unexpected: frobnitz in state: %s", frobnitz_state);

  Note: ``PW_CRASH`` is the equivalent of ``LOG_FATAL`` in other systems, where
  a device crash is triggered with a message. In Pigweed, logging and
  crashing/asserting are separated. There is a ``LOG_CRITICAL`` level in the
  logging module, but it does not have side effects; for ``LOG_FATAL``, instead
  use this macro (``PW_CRASH``).

.. cpp:function:: PW_CHECK(condition)
.. cpp:function:: PW_CHECK(condition, format, ...)

  Assert that a condition is true, optionally including a message with
  arguments to report if the codition is false.

  .. code-block:: cpp

    PW_CHECK(StartTurbines());
    PW_CHECK(StartWarpDrive(), "Oddly warp drive couldn't start; ruh-roh!");

.. cpp:function:: PW_CHECK_NOTNULL(ptr)
.. cpp:function:: PW_CHECK_NOTNULL(ptr, format, ...)

  Assert that the given pointer is not ``NULL``, optionally including a message
  with arguments to report if the pointer is ``NULL``.

  .. code-block:: cpp

    Foo* foo = GetTheFoo()
    PW_CHECK_NOTNULL(foo);

    Bar* bar = GetSomeBar()
    PW_CHECK_NOTNULL(bar, "Weirdly got NULL bar; state: %d", MyState());

.. cpp:function:: PW_CHECK_TYPE_OP(a, b)
.. cpp:function:: PW_CHECK_TYPE_OP(a, b, format, ...)

  Asserts that ``a OP b`` is true, where ``a`` and ``b`` are converted to
  ``TYPE``; with ``OP`` and ``TYPE`` described below.

  If present, the optional format message is reported on failure. Depending on
  the backend, values of ``a`` and ``b`` will also be reported.

  Example, with no message:

  .. code-block:: cpp

    PW_CHECK_INT_LE(CurrentTemperature(), 100);
    PW_CHECK_INT_LE(ItemCount(), 100);

  Example, with an included message and arguments:

  .. code-block:: cpp

    PW_CHECK_FLOAT_GE(BatteryVoltage(), 3.2, "System state=%s", SysState());

  Below is the full list of binary comparison assert macros, along with the
  type specifier. The specifier is irrelevant to application authors but is
  needed for backend implementers.

  +-------------------+--------------+-----------+-----------------------+
  | Macro             | a, b type    | condition | a, b format specifier |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_INT_LE   | int          | a <= b    | %d                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_INT_LT   | int          | a <  b    | %d                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_INT_GE   | int          | a >= b    | %d                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_INT_GT   | int          | a >  b    | %d                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_INT_EQ   | int          | a == b    | %d                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_INT_NE   | int          | a != b    | %d                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_UINT_LE  | unsigned int | a <= b    | %u                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_UINT_LT  | unsigned int | a <  b    | %u                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_UINT_GE  | unsigned int | a >= b    | %u                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_UINT_GT  | unsigned int | a >  b    | %u                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_UINT_EQ  | unsigned int | a == b    | %u                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_UINT_NE  | unsigned int | a != b    | %u                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_PTR_LE   | void*        | a <= b    | %p                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_PTR_LT   | void*        | a <  b    | %p                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_PTR_GE   | void*        | a >= b    | %p                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_PTR_GT   | void*        | a >  b    | %p                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_PTR_EQ   | void*        | a == b    | %p                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_PTR_NE   | void*        | a != b    | %p                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_FLOAT_LE | float        | a <= b    | %f                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_FLOAT_LT | float        | a <  b    | %f                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_FLOAT_GE | float        | a >= b    | %f                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_FLOAT_GT | float        | a >  b    | %f                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_FLOAT_EQ | float        | a == b    | %f                    |
  +-------------------+--------------+-----------+-----------------------+
  | PW_CHECK_FLOAT_NE | float        | a != b    | %f                    |
  +-------------------+--------------+-----------+-----------------------+

----------------------------
Assert backend API reference
----------------------------

The backend controls what to do in the case of an assertion failure. In the
most basic cases, the backend could display the assertion failure on something
like sys_io and halt in a while loop waiting for a debugger. In other cases,
the backend could store crash details like the current thread's stack to flash.

This module does not provide a backend; see ``pw_assert_basic`` for a basic
implementation (which we do not advise using in production).

Here are the macros the backend must provide:

.. cpp:function:: PW_HANDLE_CRASH(message, ...)

  The backend should trigger a system crash or halt, and if possible, deliver
  the specified message and arguments to the user or developer.

.. cpp:function:: PW_HANDLE_ASSERT_FAILURE(condition_str, message, ...)

  This macro is invoked from ``PW_CHECK`` if condition is false.  The
  application should crash with the given message and specified format
  arguments, and may optionally include the stringified condition provided in
  ``condition_str``.

.. cpp:function:: PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE( \
    a_str, a_val, op_str, b_str, b_val, type_fmt, message, ...)

  This macro is invoked from the ``PW_CHECK_*_*`` macros if the condition
  ``a_val op b_val`` is false. The facade API macros have already evaluated and
  stringified the arguments, so the backend is free to report the details as
  needed.

  The backend is expected to report the failure to the user or developer in a
  useful way, potentially capturing the string and values of the binary
  comparison operands.
