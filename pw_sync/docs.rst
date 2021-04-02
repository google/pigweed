.. _module-pw_sync:

=======
pw_sync
=======
The ``pw_sync`` module contains utilities for synchronizing between threads
and/or interrupts through signaling primitives and critical section lock
primitives.

.. contents::
   :local:
   :depth: 2

.. Warning::
  This module is still under construction, the API is not yet stable.

.. Note::
  The objects in this module do not have an Init() style public API which is
  common in many RTOS C APIs. Instead, they rely on being able to invoke the
  native initialization APIs for synchronization primitives during C++
  construction.
  In order to support global statically constructed synchronization without
  constexpr constructors, the user and/or backend **MUST** ensure that any
  initialization required in your environment is done prior to the creation
  and/or initialization of the native synchronization primitives
  (e.g. kernel initialization).

--------------------------------
Critical Section Lock Primitives
--------------------------------
The critical section lock primitives provided by this module comply with
`BasicLockable <https://en.cppreference.com/w/cpp/named_req/BasicLockable>`_,
`Lockable <https://en.cppreference.com/w/cpp/named_req/Lockable>`_, and where
relevant
`TimedLockable <https://en.cppreference.com/w/cpp/named_req/TimedLockable>`_ C++
named requirements. This means that they are compatible with existing helpers in
the STL's ``<mutex>`` thread support library. For example `std::lock_guard <https://en.cppreference.com/w/cpp/thread/lock_guard>`_
and `std::unique_lock <https://en.cppreference.com/w/cpp/thread/unique_lock>`_ can be directly used.

Mutex
=====
The Mutex is a synchronization primitive that can be used to protect shared data
from being simultaneously accessed by multiple threads. It offers exclusive,
non-recursive ownership semantics where priority inheritance is used to solve
the classic priority-inversion problem.

The Mutex's API is C++11 STL
`std::timed_mutex <https://en.cppreference.com/w/cpp/thread/timed_mutex>`_ like,
meaning it is a
`BasicLockable <https://en.cppreference.com/w/cpp/named_req/BasicLockable>`_,
`Lockable <https://en.cppreference.com/w/cpp/named_req/Lockable>`_, and
`TimedLockable <https://en.cppreference.com/w/cpp/named_req/TimedLockable>`_.

.. Warning::
  This interface will likely be split between a Mutex and TimedMutex to ease
  portability for baremetal support.

.. list-table::

  * - *Supported on*
    - *Backend module*
  * - FreeRTOS
    - :ref:`module-pw_sync_freertos`
  * - ThreadX
    - :ref:`module-pw_sync_threadx`
  * - embOS
    - :ref:`module-pw_sync_embos`
  * - STL
    - :ref:`module-pw_sync_stl`
  * - Baremetal
    - Planned
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned

C++
---
.. cpp:class:: pw::sync::Mutex

  .. cpp:function:: void lock()

     Locks the mutex, blocking indefinitely. Failures are fatal.

     **Precondition:** The lock isn't already held by this thread. Recursive
     locking is undefined behavior.

  .. cpp:function:: bool try_lock()

     Attempts to lock the mutex in a non-blocking manner.
     Returns true if the mutex was successfully acquired.

     **Precondition:** The lock isn't already held by this thread. Recursive
     locking is undefined behavior.

  .. cpp:function:: bool try_lock_for(chrono::SystemClock::duration for_at_least)

     Attempts to lock the mutex where, if needed, blocking for at least the
     specified duration.
     Returns true if the mutex was successfully acquired.

     **Precondition:** The lock isn't already held by this thread. Recursive
     locking is undefined behavior.

  .. cpp:function:: bool try_lock_until(chrono::SystemClock::time_point until_at_least)

     Attempts to lock the mutex where, if needed, blocking until at least the
     specified time_point.
     Returns true if the mutex was successfully acquired.

     **Precondition:** The lock isn't already held by this thread. Recursive
     locking is undefined behavior.

  .. cpp:function:: void unlock()

     Unlocks the mutex. Failures are fatal.

     **Precondition:** The mutex is held by this thread.

  +--------------------------------+----------+-------------+-------+
  | *Safe to use in context*       | *Thread* | *Interrupt* | *NMI* |
  +--------------------------------+----------+-------------+-------+
  | ``Mutex::Mutex``               | ✔        |             |       |
  +--------------------------------+----------+-------------+-------+
  | ``Mutex::~Mutex``              | ✔        |             |       |
  +--------------------------------+----------+-------------+-------+
  | ``void Mutex::lock``           | ✔        |             |       |
  +--------------------------------+----------+-------------+-------+
  | ``bool Mutex::try_lock``       | ✔        |             |       |
  +--------------------------------+----------+-------------+-------+
  | ``bool Mutex::try_lock_for``   | ✔        |             |       |
  +--------------------------------+----------+-------------+-------+
  | ``bool Mutex::try_lock_until`` | ✔        |             |       |
  +--------------------------------+----------+-------------+-------+
  | ``void Mutex::unlock``         | ✔        |             |       |
  +--------------------------------+----------+-------------+-------+


Examples in C++
^^^^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_chrono/system_clock.h"
  #include "pw_sync/mutex.h"

  pw::sync::Mutex mutex;

  void ThreadSafeCriticalSection() {
    mutex.lock();
    NotThreadSafeCriticalSection();
    mutex.unlock();
  }

  bool ThreadSafeCriticalSectionWithTimeout(
      const SystemClock::duration timeout) {
    if (!mutex.try_lock_for(timeout)) {
      return false;
    }
    NotThreadSafeCriticalSection();
    mutex.unlock();
    return true;
  }


Alternatively you can use C++'s RAII helpers to ensure you always unlock.

.. code-block:: cpp

  #include <mutex>

  #include "pw_chrono/system_clock.h"
  #include "pw_sync/mutex.h"

  pw::sync::Mutex mutex;

  void ThreadSafeCriticalSection() {
    std::lock_guard lock(mutex);
    NotThreadSafeCriticalSection();
  }

  bool ThreadSafeCriticalSectionWithTimeout(
      const SystemClock::duration timeout) {
    std::unique_lock lock(mutex, std::defer_lock);
    if (!lock.try_lock_for(timeout)) {
      return false;
    }
    NotThreadSafeCriticalSection();
    return true;
  }



C
-
The Mutex must be created in C++, however it can be passed into C using the
``pw_sync_Mutex`` opaque struct alias.

.. cpp:function:: void pw_sync_Mutex_Lock(pw_sync_Mutex* mutex)

  Invokes the ``Mutex::lock`` member function on the given ``mutex``.

.. cpp:function:: bool pw_sync_Mutex_TryLock(pw_sync_Mutex* mutex)

  Invokes the ``Mutex::try_lock`` member function on the given ``mutex``.

.. cpp:function:: bool pw_sync_Mutex_TryLockFor(pw_sync_Mutex* mutex, pw_chrono_SystemClock_Duration for_at_least)

  Invokes the ``Mutex::try_lock_for`` member function on the given ``mutex``.

.. cpp:function:: bool pw_sync_Mutex_TryLockUntil(pw_sync_Mutex* mutex, pw_chrono_SystemClock_TimePoint until_at_least)

  Invokes the ``Mutex::try_lock_until`` member function on the given ``mutex``.

.. cpp:function:: void pw_sync_Mutex_Unlock(pw_sync_Mutex* mutex)

  Invokes the ``Mutex::unlock`` member function on the given ``mutex``.

+-------------------------------------+----------+-------------+-------+
| *Safe to use in context*            | *Thread* | *Interrupt* | *NMI* |
+-------------------------------------+----------+-------------+-------+
| ``void pw_sync_Mutex_Lock``         | ✔        |             |       |
+-------------------------------------+----------+-------------+-------+
| ``bool pw_sync_Mutex_TryLock``      | ✔        |             |       |
+-------------------------------------+----------+-------------+-------+
| ``bool pw_sync_Mutex_TryLockFor``   | ✔        |             |       |
+-------------------------------------+----------+-------------+-------+
| ``bool pw_sync_Mutex_TryLockUntil`` | ✔        |             |       |
+-------------------------------------+----------+-------------+-------+
| ``void pw_sync_Mutex_Unlock``       | ✔        |             |       |
+-------------------------------------+----------+-------------+-------+


Example in C
^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_chrono/system_clock.h"
  #include "pw_sync/mutex.h"

  pw::sync::Mutex mutex;

  extern pw_sync_Mutex mutex;  // This can only be created in C++.

  void ThreadSafeCriticalSection(void) {
    pw_sync_Mutex_Lock(&mutex);
    NotThreadSafeCriticalSection();
    pw_sync_Mutex_Unlock(&mutex);
  }

  bool ThreadSafeCriticalSectionWithTimeout(
      const pw_chrono_SystemClock_Duration timeout) {
    if (!pw_sync_Mutex_TryLockFor(&mutex, timeout)) {
      return false;
    }
    NotThreadSafeCriticalSection();
    pw_sync_Mutex_Unlock(&mutex);
    return true;
  }


InterruptSpinLock
=================
The InterruptSpinLock is a synchronization primitive that can be used to protect
shared data from being simultaneously accessed by multiple threads and/or
interrupts as a targeted global lock, with the exception of Non-Maskable
Interrupts (NMIs). It offers exclusive, non-recursive ownership semantics where
IRQs up to a backend defined level of "NMIs" will be masked to solve
priority-inversion.

This InterruptSpinLock relies on built-in local interrupt masking to make it
interrupt safe without requiring the caller to separately mask and unmask
interrupts when using this primitive.

Unlike global interrupt locks, this also works safely and efficiently on SMP
systems. On systems which are not SMP, spinning is not required but some state
may still be used to detect recursion.

The InterruptSpinLock is a
`BasicLockable <https://en.cppreference.com/w/cpp/named_req/BasicLockable>`_
and
`Lockable <https://en.cppreference.com/w/cpp/named_req/Lockable>`_.

.. list-table::

  * - *Supported on*
    - *Backend module*
  * - FreeRTOS
    - :ref:`module-pw_sync_freertos`
  * - ThreadX
    - :ref:`module-pw_sync_threadx`
  * - embOS
    - :ref:`module-pw_sync_embos`
  * - STL
    - :ref:`module-pw_sync_stl`
  * - Baremetal
    - Planned, not ready for use
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned

C++
---
.. cpp:class:: pw::sync::InterruptSpinLock

  .. cpp:function:: void lock()

      Locks the spinlock, blocking indefinitely. Failures are fatal.

      **Precondition:** Recursive locking is undefined behavior.

  .. cpp:function:: bool try_lock()

      Attempts to lock the spinlock in a non-blocking manner.
      Returns true if the spinlock was successfully acquired.

      **Precondition:** Recursive locking is undefined behavior.

  .. cpp:function:: void unlock()

     Unlocks the mutex. Failures are fatal.

     **Precondition:** The spinlock is held by the caller.

  +-------------------------------------------+----------+-------------+-------+
  | *Safe to use in context*                  | *Thread* | *Interrupt* | *NMI* |
  +-------------------------------------------+----------+-------------+-------+
  | ``InterruptSpinLock::InterruptSpinLock``  | ✔        | ✔           |       |
  +-------------------------------------------+----------+-------------+-------+
  | ``InterruptSpinLock::~InterruptSpinLock`` | ✔        | ✔           |       |
  +-------------------------------------------+----------+-------------+-------+
  | ``void InterruptSpinLock::lock``          | ✔        | ✔           |       |
  +-------------------------------------------+----------+-------------+-------+
  | ``bool InterruptSpinLock::try_lock``      | ✔        | ✔           |       |
  +-------------------------------------------+----------+-------------+-------+
  | ``void InterruptSpinLock::unlock``        | ✔        | ✔           |       |
  +-------------------------------------------+----------+-------------+-------+


Examples in C++
^^^^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_sync/interrupt_spin_lock.h"

  pw::sync::InterruptSpinLock interrupt_spin_lock;

  void InterruptSafeCriticalSection() {
    interrupt_spin_lock.lock();
    NotThreadSafeCriticalSection();
    interrupt_spin_lock.unlock();
  }


Alternatively you can use C++'s RAII helpers to ensure you always unlock.

.. code-block:: cpp

  #include <mutex>

  #include "pw_sync/interrupt_spin_lock.h"

  pw::sync::InterruptSpinLock interrupt_spin_lock;

  void InterruptSafeCriticalSection() {
    std::lock_guard lock(interrupt_spin_lock);
    NotThreadSafeCriticalSection();
  }


C
-
The InterruptSpinLock must be created in C++, however it can be passed into C using the
``pw_sync_InterruptSpinLock`` opaque struct alias.

.. cpp:function:: void pw_sync_InterruptSpinLock_Lock(pw_sync_InterruptSpinLock* interrupt_spin_lock)

  Invokes the ``InterruptSpinLock::lock`` member function on the given ``interrupt_spin_lock``.

.. cpp:function:: bool pw_sync_InterruptSpinLock_TryLock(pw_sync_InterruptSpinLock* interrupt_spin_lock)

  Invokes the ``InterruptSpinLock::try_lock`` member function on the given ``interrupt_spin_lock``.

.. cpp:function:: void pw_sync_InterruptSpinLock_Unlock(pw_sync_InterruptSpinLock* interrupt_spin_lock)

  Invokes the ``InterruptSpinLock::unlock`` member function on the given ``interrupt_spin_lock``.

+--------------------------------------------+----------+-------------+-------+
| *Safe to use in context*                   | *Thread* | *Interrupt* | *NMI* |
+--------------------------------------------+----------+-------------+-------+
| ``void pw_sync_InterruptSpinLock_Lock``    | ✔        | ✔           |       |
+--------------------------------------------+----------+-------------+-------+
| ``bool pw_sync_InterruptSpinLock_TryLock`` | ✔        | ✔           |       |
+--------------------------------------------+----------+-------------+-------+
| ``void pw_sync_InterruptSpinLock_Unlock``  | ✔        | ✔           |       |
+--------------------------------------------+----------+-------------+-------+


Example in C
^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_chrono/system_clock.h"
  #include "pw_sync/interrupt_spin_lock.h"

  pw::sync::InterruptSpinLock interrupt_spin_lock;

  extern pw_sync_InterruptSpinLock interrupt_spin_lock;  // This can only be created in C++.

  void InterruptSafeCriticalSection(void) {
    pw_sync_InterruptSpinLock_Lock(&interrupt_spin_lock);
    NotThreadSafeCriticalSection();
    pw_sync_InterruptSpinLock_Unlock(&interrupt_spin_lock);
  }


--------------------
Signaling Primitives
--------------------

Native signaling primitives tend to vary more compared to critial section locks
across different platforms. For example, although common signaling primtives
like semaphores are in most if not all RTOSes and even POSIX, it was not in the
STL before C++20. Likewise many C++ developers are surprised that conditional
variables tend to not be natively supported on RTOSes. Although you can usually
build any signaling primitive based on other native signaling primitives, this
may come with non-trivial added overhead in ROM, RAM, and execution efficiency.

For this reason, Pigweed intends to provide some "simpler" signaling primitives
which exist to solve a narrow programming need but can be implemented as
efficiently as possible for the platform that it is used on.

This simpler but highly portable class of signaling primitives is intended to
ensure that a portability efficiency tradeoff does not have to be made up front.
For example we intend to provide a ``pw::sync::Notification`` facade which
permits a singler consumer to block until an event occurs. This should be
backed by the most efficient native primitive for a target, regardless of
whether that is a semaphore, event flag group, condition variable, or something
else.

CountingSemaphore
=================
The CountingSemaphore is a synchronization primitive that can be used for
counting events and/or resource management where receiver(s) can block on
acquire until notifier(s) signal by invoking release.

Note that unlike Mutexes, priority inheritance is not used by semaphores meaning
semaphores are subject to unbounded priority inversions. Due to this, Pigweed
does not recommend semaphores for mutual exclusion.

The CountingSemaphore is initialized to being empty or having no tokens.

The entire API is thread safe, but only a subset is interrupt safe. None of it
is NMI safe.

.. Warning::
  Releasing multiple tokens is often not natively supported, meaning you may
  end up invoking the native kernel API many times, i.e. once per token you
  are releasing!

.. list-table::

  * - *Supported on*
    - *Backend module*
  * - FreeRTOS
    - :ref:`module-pw_sync_freertos`
  * - ThreadX
    - :ref:`module-pw_sync_threadx`
  * - embOS
    - :ref:`module-pw_sync_embos`
  * - STL
    - :ref:`module-pw_sync_stl`
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned

BinarySemaphore
===============
BinarySemaphore is a specialization of CountingSemaphore with an arbitrary token
limit of 1. Note that that ``max()`` is >= 1, meaning it may be released up to
``max()`` times but only acquired once for those N releases.

Implementations of BinarySemaphore are typically more efficient than the
default implementation of CountingSemaphore.

The BinarySemaphore is initialized to being empty or having no tokens.

The entire API is thread safe, but only a subset is interrupt safe. None of it
is NMI safe.

.. list-table::

  * - *Supported on*
    - *Backend module*
  * - FreeRTOS
    - :ref:`module-pw_sync_freertos`
  * - ThreadX
    - :ref:`module-pw_sync_threadx`
  * - embOS
    - :ref:`module-pw_sync_embos`
  * - STL
    - :ref:`module-pw_sync_stl`
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned

Coming Soon
===========
We are intending to provide facades for:

* ``pw::sync::Notification``: A portable abstraction to allow threads to receive
  notification of a single occurrence of a single event.

* ``pw::sync::EventGroup`` A facade for a common primitive on RTOSes like
  FreeRTOS, RTX5, ThreadX, and embOS which permit threads and interrupts to
  signal up to 32 events. This permits others threads to be notified when either
  any or some combination of these events have been signaled. This is frequently
  used as an alternative to a set of binary semaphore(s). This is not supported
  natively on Zephyr.
