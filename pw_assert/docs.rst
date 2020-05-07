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

.. tip::

  All macros have both a ``CHECK`` and ``DCHECK`` variant. The ``CHECK``
  variant is always enabled, even in production. Generally, we advise making
  most asserts ``CHECK`` rather than ``DCHECK``, unless there is a critical
  performance or code size reason to use ``DCHECK``.

  .. code-block:: cpp

    // This assert is always enabled, even in production.
    PW_CHECK_INT_LE(ItemCount(), 100);

    // This assert disabled for release builds, where NDEBUG is defined.
    // The functions ItemCount() and GetStateStr() are never called.
    PW_DCHECK_INT_LE(ItemCount(), 100, "System state: %s", GetStateStr());

Design discussion
-----------------
The Pigweed assert API was designed taking into account the needs of several
past projects the team members were involved with. Based on those experiences,
the following were key requirements for the API:

1. **C compatibility** - Since asserts are typically invoked from arbitrary
   contexts, including from vendor or third party code, the assert system must
   have a C-compatible API. Some API functions working only in C++ is
   acceptable, as long as the key functions work in C.
2. **Capturing both expressions and values** - Since asserts can trigger in
   ways that are not repeatable, it is important to capture rich diagnostic
   information to help identifying the root cause of the fault. For asserts,
   this means including the failing expression text, and optionally also
   capturing failing expression values. For example, instead of capturing an
   error with the expression (``x < y``), capturing an error with the
   expression and values(``x < y, with x = 10, y = 0``).
3. **Tokenization compatible** - It's important that the assert expressions
   support tokenization; both the expression itself (e.g. ``a < b``) and the
   message attached to the expression. For example: ``PW_CHECK(ItWorks(), "Ruh
   roh: %d", some_int)``.
4. **Customizable assert handling** - Most products need to support custom
   handling of asserts. In some cases, an assert might trigger printing out
   details to a UART; in other cases, it might trigger saving a log entry to
   flash. The assert system must support this customization.

The combination of #1, #2, and #3 led to the structure of the API. In
particular, the need to support tokenized asserts and the need to support
capturing values led to the choice of having ``PW_CHECK_INT_LE(a, b)`` instead
of ``PW_CHECK(a <= b)``. Needing to support tokenization is what drove the
facade & backend arrangement, since the backend must provide the raw macros for
asserting in that case, rather than terminating at a C-style API.

Why isn't there a ``PW_CHECK_LE``? Why is the type (e.g. ``INT``) needed?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The problem with asserts like ``PW_CHECK_LE(a, b)`` instead of
``PW_CHECK_INT_LE(a, b)`` or ``PW_CHECK_FLOAT_LE(a, b)`` is that to capture the
arguments with the tokenizer, we need to know the types. Using the
preprocessor, it is impossible to dispatch based on the types of ``a`` and
``b``, so unfortunately having a separate macro for each of the types commonly
asserted on is necessary.

How should objects be asserted against or compared?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Unfortunatly, there is no native mechanism for this, and instead the way to
assert object states or comparisons is with the normal ``PW_CHECK_*`` macros
that operate on booleans, ints, and floats.

This is due to the requirement of supporting C and also tokenization. It may be
possible support rich object comparions by defining a convention for
stringifying objects; however, this hasn't been added yet. Additionally, such a
mechanism would not work well with tokenization. In particular, it would
require runtime stringifying arguments and rendering them with ``%s``, which
leads to binary bloat even with tokenization. So it is likely that a rich
object assert API won't be added.

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
.. cpp:function:: PW_DCHECK(condition)
.. cpp:function:: PW_DCHECK(condition, format, ...)

  Assert that a condition is true, optionally including a message with
  arguments to report if the codition is false.

  The ``DCHECK`` variants only run if ``NDEBUG`` is defined; otherwise, the
  entire statement is removed (and the expression not evaluated).

  Example:

  .. code-block:: cpp

    PW_CHECK(StartTurbines());
    PW_CHECK(StartWarpDrive(), "Oddly warp drive couldn't start; ruh-roh!");
    PW_CHECK(RunSelfTest(), "Failure in self test; try %d", TestAttempts());

  .. attention::

    Don't use use ``PW_CHECK`` for binary comparisons or status checks!

    Instead, use the ``PW_CHECK_<TYPE>_<OP>`` macros. These macros enable
    capturing the value of the operands, and also tokenizing them if using a
    tokenizing assert backend. For example, if ``x`` and ``b`` are integers,
    use instead ``PW_CHECK_INT_LT(x, b)``.

    Additionally, use ``PW_CHECK_OK(status)`` when checking for a
    ``Status::OK``, since it enables showing a human-readable status string
    rather than an integer (e.g. ``status == RESOURCE_EXHAUSTED`` instead of
    ``status == 5``.

    +------------------------------------+-------------------------------------+
    | **Do NOT do this**                 | **Do this instead**                 |
    +------------------------------------+-------------------------------------+
    | ``PW_CHECK(a_int < b_int)``        | ``PW_CHECK_INT_LT(a_int, b_int)``   |
    +------------------------------------+-------------------------------------+
    | ``PW_CHECK(a_ptr <= b_ptr)``       | ``PW_CHECK_PTR_LE(a_ptr, b_ptr)``   |
    +------------------------------------+-------------------------------------+
    | ``PW_CHECK(Temp() <= 10.0)``       | ``PW_CHECK_FLOAT_LE(Temp(), 10.0)`` |
    +------------------------------------+-------------------------------------+
    | ``PW_CHECK(Foo() == Status::OK)``  | ``PW_CHECK_OK(Foo())``              |
    +------------------------------------+-------------------------------------+

.. cpp:function:: PW_CHECK_NOTNULL(ptr)
.. cpp:function:: PW_CHECK_NOTNULL(ptr, format, ...)
.. cpp:function:: PW_DCHECK_NOTNULL(ptr)
.. cpp:function:: PW_DCHECK_NOTNULL(ptr, format, ...)

  Assert that the given pointer is not ``NULL``, optionally including a message
  with arguments to report if the pointer is ``NULL``.

  The ``DCHECK`` variants only run if ``NDEBUG`` is defined; otherwise, the
  entire statement is removed (and the expression not evaluated).

  .. code-block:: cpp

    Foo* foo = GetTheFoo()
    PW_CHECK_NOTNULL(foo);

    Bar* bar = GetSomeBar();
    PW_CHECK_NOTNULL(bar, "Weirdly got NULL bar; state: %d", MyState());

.. cpp:function:: PW_CHECK_TYPE_OP(a, b)
.. cpp:function:: PW_CHECK_TYPE_OP(a, b, format, ...)
.. cpp:function:: PW_DCHECK_TYPE_OP(a, b)
.. cpp:function:: PW_DCHECK_TYPE_OP(a, b, format, ...)

  Asserts that ``a OP b`` is true, where ``a`` and ``b`` are converted to
  ``TYPE``; with ``OP`` and ``TYPE`` described below.

  If present, the optional format message is reported on failure. Depending on
  the backend, values of ``a`` and ``b`` will also be reported.

  The ``DCHECK`` variants only run if ``NDEBUG`` is defined; otherwise, the
  entire statement is removed (and the expression not evaluated).

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

  The above ``CHECK_*_*()`` are also available in DCHECK variants, which will
  only evaluate their arguments and trigger if the ``NDEBUG`` macro is defined.

  +--------------------+--------------+-----------+-----------------------+
  | Macro              | a, b type    | condition | a, b format specifier |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_INT_LE   | int          | a <= b    | %d                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_INT_LT   | int          | a <  b    | %d                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_INT_GE   | int          | a >= b    | %d                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_INT_GT   | int          | a >  b    | %d                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_INT_EQ   | int          | a == b    | %d                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_INT_NE   | int          | a != b    | %d                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_UINT_LE  | unsigned int | a <= b    | %u                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_UINT_LT  | unsigned int | a <  b    | %u                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_UINT_GE  | unsigned int | a >= b    | %u                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_UINT_GT  | unsigned int | a >  b    | %u                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_UINT_EQ  | unsigned int | a == b    | %u                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_UINT_NE  | unsigned int | a != b    | %u                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_PTR_LE   | void*        | a <= b    | %p                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_PTR_LT   | void*        | a <  b    | %p                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_PTR_GE   | void*        | a >= b    | %p                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_PTR_GT   | void*        | a >  b    | %p                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_PTR_EQ   | void*        | a == b    | %p                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_PTR_NE   | void*        | a != b    | %p                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_FLOAT_LE | float        | a <= b    | %f                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_FLOAT_LT | float        | a <  b    | %f                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_FLOAT_GE | float        | a >= b    | %f                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_FLOAT_GT | float        | a >  b    | %f                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_FLOAT_EQ | float        | a == b    | %f                    |
  +--------------------+--------------+-----------+-----------------------+
  | PW_DCHECK_FLOAT_NE | float        | a != b    | %f                    |
  +--------------------+--------------+-----------+-----------------------+

.. cpp:function:: PW_CHECK_OK(status)
.. cpp:function:: PW_CHECK_OK(status, format, ...)
.. cpp:function:: PW_DCHECK_OK(status)
.. cpp:function:: PW_DCHECK_OK(status, format, ...)

  Assert that ``status`` evaluates to ``pw::Status::OK`` (in C++) or
  ``PW_STATUS_OK`` (in C). Optionally include a message with arguments to
  report.

  The ``DCHECK`` variants only run if ``NDEBUG`` is defined; otherwise, the
  entire statement is removed (and the expression not evaluated).

  .. code-block:: cpp

    pw::Status operation_status = DoSomeOperation();
    PW_CHECK_OK(operation_status);

    // Any expression that evaluates to a pw::Status or pw_Status works.
    PW_CHECK_OK(DoTheThing(), "System state: %s", SystemState());

    // C works too.
    pw_Status c_status = DoMoreThings();
    PW_CHECK_OK(c_status, "System state: %s", SystemState());

  .. note::

    Using ``PW_CHECK_OK(status)`` instead of ``PW_CHECK(status == Status::OK)``
    enables displaying an error message with a string version of the error
    code; for example ``status == RESOURCE_EXHAUSTED`` instead of ``status ==
    5``.

----------------------------
Assert backend API reference
----------------------------

The backend controls what to do in the case of an assertion failure. In the
most basic cases, the backend could display the assertion failure on something
like sys_io and halt in a while loop waiting for a debugger. In other cases,
the backend could store crash details like the current thread's stack to flash.

This facade module (``pw_assert``) does not provide a backend. See
:ref:`chapter-pw-assert-basic` for a basic implementation.

The backend must provide the header

``pw_assert_backend/backend.h``

and that header must define the following macros:

.. cpp:function:: PW_HANDLE_CRASH(message, ...)

  Trigger a system crash or halt, and if possible, deliver the specified
  message and arguments to the user or developer.

.. cpp:function:: PW_HANDLE_ASSERT_FAILURE(condition_str, message, ...)

  Trigger a system crash or halt, and if possible, deliver the condition string
  (indicating what expression was false) and the message with format arguments,
  to the user or developer.

  This macro is invoked from the ``PW_CHECK`` facade macro if condition is
  false.

.. cpp:function:: PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE( \
    a_str, a_val, op_str, b_str, b_val, type_fmt, message, ...)

  Trigger a system crash or halt for a failed binary comparison assert (e.g.
  any of the ``PW_CHECK_<type>_<op>`` macros). The handler should combine the
  assert components into a useful message for the user; though in some cases
  this may not be possible.

  Consider the following example:

  .. code-block:: cpp

    int temp = 16;
    int max_temp = 15;
    PW_CHECK_INT_LE(temp, MAX_TEMP, "Got too hot; state: %s", GetSystemState());

  In this block, the assert will trigger, which will cause the facade to invoke
  the handler macro. Below is the meaning of the arguments, referencing to the
  example:

  - ``a_str`` - Stringified first operand. In the example: ``"temp"``.
  - ``a_val`` - The value of the first operand. In the example: ``16``.
  - ``op_str`` - The string version of the operator. In the example: "<=".
  - ``b_str`` - Stringified second operand. In the example: ``"max_temp"``.
  - ``b_val`` - The value of the second operand. In the example: ``15``.
  - ``type_fmt`` - The format code for the type. In the example: ``"%d"``.
  - ``message, ...`` - A formatted message to go with the assert. In the
    example: ``"Got too hot; state: %s", "ON_FIRE"``.

  .. tip::

    See :ref:`chapter-pw-assert-basic` for one way to combine these arguments
    into a meaningful error message.

.. attention::

  The facade macros (``PW_CRASH`` and related) are expected to behave like they
  have the ``[[ noreturn ]]`` attribute set. This implies that the backend
  handler functions, ``PW_HANDLE_*`` defined by the backend, must not return.

  In other words, the device must reboot.

-------------
Compatibility
-------------
The facade is compatible with C and C++.
