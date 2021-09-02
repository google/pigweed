.. _module-pw_chrono:

=========
pw_chrono
=========
Pigweed's chrono module provides facilities for applications to deal with time,
leveraging many pieces of STL's the ``std::chrono`` library but with a focus
on portability for constrained embedded devices and maintaining correctness.

At a high level Pigweed's time primitives rely on C++'s
`<chrono> <https://en.cppreference.com/w/cpp/header/chrono>`_ library to enable
users to express intents with strongly typed real time units. In addition, it
extends the C++ named
`Clock <https://en.cppreference.com/w/cpp/named_req/Clock>`_ and
`TrivialClock <https://en.cppreference.com/w/cpp/named_req/TrivialClock>`_
requirements with additional attributes such as whether a clock is monotonic
(not just steady), is always enabled (or requires enabling), is free running
(works even if interrupts are masked), whether it is safe to use in a
Non-Maskable Interrupts (NMI), what the epoch is, and more.

.. warning::
  This module is still under construction, the API is not yet stable. Also the
  documentation is incomplete.

------------------
SystemClock facade
------------------
The ``pw::chrono::SystemClock`` is meant to serve as the clock used for time
bound operations such as thread sleeping, waiting on mutexes/semaphores, etc.
The ``SystemClock`` always uses a signed 64 bit as the underlying type for time
points and durations. This means users do not have to worry about clock overflow
risk as long as rational durations and time points as used, i.e. within a range
of ±292 years.

------------------
SystemTimer facade
------------------
The SystemTimer facade enables deferring execution of a callback until a later
time. For example, enabling low power mode after a period of inactivity.

The base SystemTimer only supports a one-shot style timer with a callback.
A periodic timer can be implemented by rescheduling the timer in the callback
through ``InvokeAt(kDesiredPeriod + expired_deadline)``.

When implementing a periodic layer on top, the user should be mindful of
handling missed periodic callbacks. They could opt to invoke the callback
multiple times with the expected ``expired_deadline`` values or instead saturate
and invoke the callback only once with the latest ``expired_deadline``.

The entire API is thread safe, however it is NOT always IRQ safe.

The ExpiryCallback is either invoked from a high priority thread or an
interrupt. Ergo ExpiryCallbacks should be treated as if they are executed by an
interrupt, meaning:

 * Processing inside of the callback should be kept to a minimum.

 * Callbacks should never attempt to block.

 * APIs which are not interrupt safe such as pw::sync::Mutex should not be used!

C++
---
.. cpp:class:: pw::chrono::SystemTimer

  .. cpp:function:: SystemTimer(ExpiryCallback callback)

    Constructs the SystemTimer based on the user provided
    ``pw::Function<void(SystemClock::time_point expired_deadline)>``. Note that
    The ExpiryCallback is either invoked from a high priority thread or an
    interrupt.

    .. note::
      For a given timer instance, its ExpiryCallback will not preempt itself.
      This makes it appear like there is a single executor of a timer instance's
      ExpiryCallback.

  .. cpp:function:: void InvokeAfter(chrono::SystemClock::duration delay)

    Invokes the expiry callback as soon as possible after at least the
    specified duration.

    Scheduling a callback cancels the existing callback (if pending).
    If the callback is already being executed while you reschedule it, it will
    finish callback execution to completion. You are responsible for any
    critical section locks which may be needed for timer coordination.

    This is thread safe, it may not be IRQ safe.

  .. cpp:function:: void InvokeAt(chrono::SystemClock::time_point timestamp)

    Invokes the expiry callback as soon as possible starting at the specified
    time_point.

    Scheduling a callback cancels the existing callback (if pending).
    If the callback is already being executed while you reschedule it, it will
    finish callback execution to completion. You are responsible for any
    critical section locks which may be needed for timer coordination.

    This is thread safe, it may not be IRQ safe.

  .. cpp:function:: void Cancel()

    Cancels the software timer expiry callback if pending.

    Canceling a timer which isn't scheduled does nothing.

    If the callback is already being executed while you cancel it, it will
    finish callback execution to completion. You are responsible for any
    synchronization which is needed for thread safety.

    This is thread safe, it may not be IRQ safe.

  .. list-table::

    * - *Safe to use in context*
      - *Thread*
      - *Interrupt*
      - *NMI*
    * - ``SystemTimer::SystemTimer``
      - ✔
      -
      -
    * - ``SystemTimer::~SystemTimer``
      - ✔
      -
      -
    * - ``void SystemTimer::InvokeAfter``
      - ✔
      -
      -
    * - ``void SystemTimer::InvokeAt``
      - ✔
      -
      -
    * - ``void SystemTimer::Cancel``
      - ✔
      -
      -

Examples in C++
^^^^^^^^^^^^^^^

.. code-block:: cpp

  #include "pw_chrono/system_clock.h"
  #include "pw_chrono/system_timer.h"
  #include "pw_log/log.h"

  using namespace std::chrono_literals;

  void DoFoo(pw::chrono::SystemClock::time_point expired_deadline) {
    PW_LOG_INFO("Timer callback invoked!");
  }

  pw::chrono::SystemTimer foo_timer(DoFoo);

  void DoFooLater() {
    foo_timer.InvokeAfter(42ms);  // DoFoo will be invoked after 42ms.
  }
