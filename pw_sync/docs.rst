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
`std::mutex <https://en.cppreference.com/w/cpp/thread/mutex>`_ like,
meaning it is a
`BasicLockable <https://en.cppreference.com/w/cpp/named_req/BasicLockable>`_
and `Lockable <https://en.cppreference.com/w/cpp/named_req/Lockable>`_.

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

  .. cpp:function:: void unlock()

     Unlocks the mutex. Failures are fatal.

     **Precondition:** The mutex is held by this thread.


  .. list-table::

    * - *Safe to use in context*
      - *Thread*
      - *Interrupt*
      - *NMI*
    * - ``Mutex::Mutex``
      - ✔
      -
      -
    * - ``Mutex::~Mutex``
      - ✔
      -
      -
    * - ``void Mutex::lock``
      - ✔
      -
      -
    * - ``bool Mutex::try_lock``
      - ✔
      -
      -
    * - ``void Mutex::unlock``
      - ✔
      -
      -

Examples in C++
^^^^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_sync/mutex.h"

  pw::sync::Mutex mutex;

  void ThreadSafeCriticalSection() {
    mutex.lock();
    NotThreadSafeCriticalSection();
    mutex.unlock();
  }


Alternatively you can use C++'s RAII helpers to ensure you always unlock.

.. code-block:: cpp

  #include <mutex>

  #include "pw_sync/mutex.h"

  pw::sync::Mutex mutex;

  void ThreadSafeCriticalSection() {
    std::lock_guard lock(mutex);
    NotThreadSafeCriticalSection();
  }


C
-
The Mutex must be created in C++, however it can be passed into C using the
``pw_sync_Mutex`` opaque struct alias.

.. cpp:function:: void pw_sync_Mutex_Lock(pw_sync_Mutex* mutex)

  Invokes the ``Mutex::lock`` member function on the given ``mutex``.

.. cpp:function:: bool pw_sync_Mutex_TryLock(pw_sync_Mutex* mutex)

  Invokes the ``Mutex::try_lock`` member function on the given ``mutex``.

.. cpp:function:: void pw_sync_Mutex_Unlock(pw_sync_Mutex* mutex)

  Invokes the ``Mutex::unlock`` member function on the given ``mutex``.

.. list-table::

  * - *Safe to use in context*
    - *Thread*
    - *Interrupt*
    - *NMI*
  * - ``void pw_sync_Mutex_Lock``
    - ✔
    -
    -
  * - ``bool pw_sync_Mutex_TryLock``
    - ✔
    -
    -
  * - ``void pw_sync_Mutex_Unlock``
    - ✔
    -
    -

Example in C
^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_sync/mutex.h"

  pw::sync::Mutex mutex;

  extern pw_sync_Mutex mutex;  // This can only be created in C++.

  void ThreadSafeCriticalSection(void) {
    pw_sync_Mutex_Lock(&mutex);
    NotThreadSafeCriticalSection();
    pw_sync_Mutex_Unlock(&mutex);
  }

TimedMutex
==========
The TimedMutex is an extension of the Mutex which offers timeout and deadline
based semantics.

The TimedMutex's API is C++11 STL
`std::timed_mutex <https://en.cppreference.com/w/cpp/thread/timed_mutex>`_ like,
meaning it is a
`BasicLockable <https://en.cppreference.com/w/cpp/named_req/BasicLockable>`_,
`Lockable <https://en.cppreference.com/w/cpp/named_req/Lockable>`_, and
`TimedLockable <https://en.cppreference.com/w/cpp/named_req/TimedLockable>`_.

Note that the ``TimedMutex`` is a derived ``Mutex`` class, meaning that
a ``TimedMutex`` can be used by someone who needs the basic ``Mutex``. This is
in stark contrast to the C++ STL's
`std::timed_mutex <https://en.cppreference.com/w/cpp/thread/timed_mutex>`_.


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

C++
---
.. cpp:class:: pw::sync::TimedMutex

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


  .. list-table::

    * - *Safe to use in context*
      - *Thread*
      - *Interrupt*
      - *NMI*
    * - ``TimedMutex::TimedMutex``
      - ✔
      -
      -
    * - ``TimedMutex::~TimedMutex``
      - ✔
      -
      -
    * - ``void TimedMutex::lock``
      - ✔
      -
      -
    * - ``bool TimedMutex::try_lock``
      - ✔
      -
      -
    * - ``bool TimedMutex::try_lock_for``
      - ✔
      -
      -
    * - ``bool TimedMutex::try_lock_until``
      - ✔
      -
      -
    * - ``void TimedMutex::unlock``
      - ✔
      -
      -

Examples in C++
^^^^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_chrono/system_clock.h"
  #include "pw_sync/timed_mutex.h"

  pw::sync::TimedMutex mutex;

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
  #include "pw_sync/timed_mutex.h"

  pw::sync::TimedMutex mutex;

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
The TimedMutex must be created in C++, however it can be passed into C using the
``pw_sync_TimedMutex`` opaque struct alias.

.. cpp:function:: void pw_sync_TimedMutex_Lock(pw_sync_TimedMutex* mutex)

  Invokes the ``TimedMutex::lock`` member function on the given ``mutex``.

.. cpp:function:: bool pw_sync_TimedMutex_TryLock(pw_sync_TimedMutex* mutex)

  Invokes the ``TimedMutex::try_lock`` member function on the given ``mutex``.

.. cpp:function:: bool pw_sync_TimedMutex_TryLockFor(pw_sync_TimedMutex* mutex, pw_chrono_SystemClock_Duration for_at_least)

  Invokes the ``TimedMutex::try_lock_for`` member function on the given ``mutex``.

.. cpp:function:: bool pw_sync_TimedMutex_TryLockUntil(pw_sync_TimedMutex* mutex, pw_chrono_SystemClock_TimePoint until_at_least)

  Invokes the ``TimedMutex::try_lock_until`` member function on the given ``mutex``.

.. cpp:function:: void pw_sync_TimedMutex_Unlock(pw_sync_TimedMutex* mutex)

  Invokes the ``TimedMutex::unlock`` member function on the given ``mutex``.

.. list-table::

  * - *Safe to use in context*
    - *Thread*
    - *Interrupt*
    - *NMI*
  * - ``void pw_sync_TimedMutex_Lock``
    - ✔
    -
    -
  * - ``bool pw_sync_TimedMutex_TryLock``
    - ✔
    -
    -
  * - ``bool pw_sync_TimedMutex_TryLockFor``
    - ✔
    -
    -
  * - ``bool pw_sync_TimedMutex_TryLockUntil``
    - ✔
    -
    -
  * - ``void pw_sync_TimedMutex_Unlock``
    - ✔
    -
    -

Example in C
^^^^^^^^^^^^
.. code-block:: cpp

  #include "pw_chrono/system_clock.h"
  #include "pw_sync/timed_mutex.h"

  pw::sync::TimedMutex mutex;

  extern pw_sync_TimedMutex mutex;  // This can only be created in C++.

  bool ThreadSafeCriticalSectionWithTimeout(
      const pw_chrono_SystemClock_Duration timeout) {
    if (!pw_sync_TimedMutex_TryLockFor(&mutex, timeout)) {
      return false;
    }
    NotThreadSafeCriticalSection();
    pw_sync_TimedMutex_Unlock(&mutex);
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

  .. list-table::

    * - *Safe to use in context*
      - *Thread*
      - *Interrupt*
      - *NMI*
    * - ``InterruptSpinLock::InterruptSpinLock``
      - ✔
      - ✔
      -
    * - ``InterruptSpinLock::~InterruptSpinLock``
      - ✔
      - ✔
      -
    * - ``void InterruptSpinLock::lock``
      - ✔
      - ✔
      -
    * - ``bool InterruptSpinLock::try_lock``
      - ✔
      - ✔
      -
    * - ``void InterruptSpinLock::unlock``
      - ✔
      - ✔
      -

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

.. list-table::

  * - *Safe to use in context*
    - *Thread*
    - *Interrupt*
    - *NMI*
  * - ``void pw_sync_InterruptSpinLock_Lock``
    - ✔
    - ✔
    -
  * - ``bool pw_sync_InterruptSpinLock_TryLock``
    - ✔
    - ✔
    -
  * - ``void pw_sync_InterruptSpinLock_Unlock``
    - ✔
    - ✔
    -

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

Thread Safety Lock Annotations
==============================
Pigweed's critical section lock primitives support Clang's thread safety
analysis extension for C++. The analysis is completely static at compile-time.
This is only supported when building with Clang. The annotations are no-ops when
using different compilers.

Pigweed provides the ``pw_sync/lock_annotations.h`` header file with macro
definitions to allow developers to document the locking policies of
multi-threaded code. The annotations can also help program analysis tools to
identify potential thread safety issues.

More information on Clang's thread safety analysis system can be found
`here <https://clang.llvm.org/docs/ThreadSafetyAnalysis.html>`_.

Enabling Clang's Analysis
-------------------------
In order to enable the analysis, Clang requires that the ``-Wthread-safety``
compilation flag be used. In addition, if any STL components like
``std::lock_guard`` are used, the STL's built in annotations have to be manually
enabled, typically by setting the ``_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS``
macro.

If using GN, the ``pw_build:clang_thread_safety_warnings`` config is provided
to do this for you, when added to your clang toolchain definition's default
configs.

Why use lock annotations?
-------------------------
Lock annotations can help warn you about potential race conditions in your code
when using locks: you have to remember to grab lock(s) before entering a
critical section, yuou have to remember to unlock it when you leave, and you
have to avoid deadlocks.

Clang's lock annotations let you inform the compiler and anyone reading your
code which variables are guarded by which locks, which locks should or cannot be
held when calling which function, which order locks should be acquired in, etc.

Using Lock Annotations
----------------------
When referring to locks in the arguments of the attributes, you should
use variable names or more complex expressions (e.g. ``my_object->lock_``)
that evaluate to a concrete lock object whenever possible. If the lock
you want to refer to is not in scope, you may use a member pointer
(e.g. ``&MyClass::lock_``) to refer to a lock in some (unknown) object.

Annotating Lock Usage
^^^^^^^^^^^^^^^^^^^^^
.. cpp:function:: PW_GUARDED_BY(x)

  Documents if a shared field or global variable needs to be protected by a
  lock. ``PW_GUARDED_BY()`` allows the user to specify a particular lock that
  should be held when accessing the annotated variable.

  Although this annotation (and ``PW_PT_GUARDED_BY``, below) cannot be applied
  to local variables, a local variable and its associated lock can often be
  combined into a small class or struct, thereby allowing the annotation.

  Example:

  .. code-block:: cpp

    class Foo {
      Mutex mu_;
      int p1_ PW_GUARDED_BY(mu_);
      ...
    };

.. cpp:function:: PW_PT_GUARDED_BY(x)

  Documents if the memory location pointed to by a pointer should be guarded
  by a lock when dereferencing the pointer.

  Example:

  .. code-block:: cpp

    class Foo {
      Mutex mu_;
      int *p1_ PW_PT_GUARDED_BY(mu_);
      ...
    };

  Note that a pointer variable to a shared memory location could itself be a
  shared variable.

  Example:

  .. code-block:: cpp

    // `q_`, guarded by `mu1_`, points to a shared memory location that is
    // guarded by `mu2_`:
    int *q_ PW_GUARDED_BY(mu1_) PW_PT_GUARDED_BY(mu2_);

.. cpp:function:: PW_ACQUIRED_AFTER(...)
.. cpp:function:: PW_ACQUIRED_BEFORE(...)

  Documents the acquisition order between locks that can be held
  simultaneously by a thread. For any two locks that need to be annotated
  to establish an acquisition order, only one of them needs the annotation.
  (i.e. You don't have to annotate both locks with both ``PW_ACQUIRED_AFTER``
  and ``PW_ACQUIRED_BEFORE``.)

  As with ``PW_GUARDED_BY``, this is only applicable to locks that are shared
  fields or global variables.

  Example:

  .. code-block:: cpp

    Mutex m1_;
    Mutex m2_ PW_ACQUIRED_AFTER(m1_);

.. cpp:function:: PW_EXCLUSIVE_LOCKS_REQUIRED(...)
.. cpp:function:: PW_SHARED_LOCKS_REQUIRED(...)

  Documents a function that expects a lock to be held prior to entry.
  The lock is expected to be held both on entry to, and exit from, the
  function.

  An exclusive lock allows read-write access to the guarded data member(s), and
  only one thread can acquire a lock exclusively at any one time. A shared lock
  allows read-only access, and any number of threads can acquire a shared lock
  concurrently.

  Generally, non-const methods should be annotated with
  ``PW_EXCLUSIVE_LOCKS_REQUIRED``, while const methods should be annotated with
  ``PW_SHARED_LOCKS_REQUIRED``.

  Example:

  .. code-block:: cpp

    Mutex mu1, mu2;
    int a PW_GUARDED_BY(mu1);
    int b PW_GUARDED_BY(mu2);

    void foo() PW_EXCLUSIVE_LOCKS_REQUIRED(mu1, mu2) { ... }
    void bar() const PW_SHARED_LOCKS_REQUIRED(mu1, mu2) { ... }

.. cpp:function:: PW_LOCKS_EXCLUDED(...)

  Documents the locks acquired in the body of the function. These locks
  cannot be held when calling this function (as Pigweed's default locks are
  non-reentrant).

  Example:

  .. code-block:: cpp

    Mutex mu;
    int a PW_GUARDED_BY(mu);

    void foo() PW_LOCKS_EXCLUDED(mu) {
      mu.lock();
      ...
      mu.unlock();
    }

.. cpp:function:: PW_LOCK_RETURNED(...)

  Documents a function that returns a lock without acquiring it.  For example,
  a public getter method that returns a pointer to a private lock should
  be annotated with ``PW_LOCK_RETURNED``.

  Example:

  .. code-block:: cpp

    class Foo {
     public:
      Mutex* mu() PW_LOCK_RETURNED(mu) { return &mu; }

     private:
      Mutex mu;
    };

.. cpp:function:: PW_NO_LOCK_SAFETY_ANALYSIS()

   Turns off thread safety checking within the body of a particular function.
   This annotation is used to mark functions that are known to be correct, but
   the locking behavior is more complicated than the analyzer can handle.

Annotating Lock Objects
^^^^^^^^^^^^^^^^^^^^^^^
In order of lock usage annotation to work, the lock objects themselves need to
be annotated as well. In case you are providing your own lock or psuedo-lock
object, you can use the macros in this section to annotate it.

As an example we've annotated a Lock and a RAII ScopedLocker object for you, see
the macro documentation after for more details:

.. code-block:: cpp

  class PW_LOCKABLE("Lock") Lock {
   public:
    void Lock() PW_EXCLUSIVE_LOCK_FUNCTION();

    void ReaderLock() PW_SHARED_LOCK_FUNCTION();

    void Unlock() PW_UNLOCK_FUNCTION();

    void ReaderUnlock() PW_SHARED_TRYLOCK_FUNCTION();

    bool TryLock() PW_EXCLUSIVE_TRYLOCK_FUNCTION(true);

    bool ReaderTryLock() PW_SHARED_TRYLOCK_FUNCTION(true);

    void AssertHeld() PW_ASSERT_EXCLUSIVE_LOCK();

    void AssertReaderHeld() PW_ASSERT_SHARED_LOCK();
  };


  // Tag types for selecting a constructor.
  struct adopt_lock_t {} inline constexpr adopt_lock = {};
  struct defer_lock_t {} inline constexpr defer_lock = {};
  struct shared_lock_t {} inline constexpr shared_lock = {};

  class PW_SCOPED_LOCKABLE ScopedLocker {
    // Acquire lock, implicitly acquire *this and associate it with lock.
    ScopedLocker(Lock *lock) PW_EXCLUSIVE_LOCK_FUNCTION(lock)
        : lock_(lock), locked(true) {
      lock->Lock();
    }

    // Assume lock is held, implicitly acquire *this and associate it with lock.
    ScopedLocker(Lock *lock, adopt_lock_t) PW_EXCLUSIVE_LOCKS_REQUIRED(lock)
        : lock_(lock), locked(true) {}

    // Acquire lock in shared mode, implicitly acquire *this and associate it
    // with lock.
    ScopedLocker(Lock *lock, shared_lock_t) PW_SHARED_LOCK_FUNCTION(lock)
        : lock_(lock), locked(true) {
      lock->ReaderLock();
    }

    // Assume lock is held in shared mode, implicitly acquire *this and associate
    // it with lock.
    ScopedLocker(Lock *lock, adopt_lock_t, shared_lock_t)
        PW_SHARED_LOCKS_REQUIRED(lock) : lock_(lock), locked(true) {}

    // Assume lock is not held, implicitly acquire *this and associate it with
    // lock.
    ScopedLocker(Lock *lock, defer_lock_t) PW_LOCKS_EXCLUDED(lock)
        : lock_(lock), locked(false) {}

    // Release *this and all associated locks, if they are still held.
    // There is no warning if the scope was already unlocked before.
    ~ScopedLocker() PW_UNLOCK_FUNCTION() {
      if (locked)
        lock_->GenericUnlock();
    }

    // Acquire all associated locks exclusively.
    void Lock() PW_EXCLUSIVE_LOCK_FUNCTION() {
      lock_->Lock();
      locked = true;
    }

    // Try to acquire all associated locks exclusively.
    bool TryLock() PW_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
      return locked = lock_->TryLock();
    }

    // Acquire all associated locks in shared mode.
    void ReaderLock() PW_SHARED_LOCK_FUNCTION() {
      lock_->ReaderLock();
      locked = true;
    }

    // Try to acquire all associated locks in shared mode.
    bool ReaderTryLock() PW_SHARED_TRYLOCK_FUNCTION(true) {
      return locked = lock_->ReaderTryLock();
    }

    // Release all associated locks. Warn on double unlock.
    void Unlock() PW_UNLOCK_FUNCTION() {
      lock_->Unlock();
      locked = false;
    }

    // Release all associated locks. Warn on double unlock.
    void ReaderUnlock() PW_UNLOCK_FUNCTION() {
      lock_->ReaderUnlock();
      locked = false;
    }

   private:
    Lock* lock_;
    bool locked_;
  };

.. cpp:function:: PW_LOCKABLE(name)

  Documents if a class/type is a lockable type (such as the ``pw::sync::Mutex``
  class). The name is used in the warning messages. This can also be useful on
  classes which have locking like semantics but aren't actually locks.

.. cpp:function:: PW_SCOPED_LOCKABLE()

  Documents if a class does RAII locking. The name is used in the warning
  messages.

  The constructor should use ``LOCK_FUNCTION()`` to specify the lock that is
  acquired, and the destructor should use ``UNLOCK_FUNCTION()`` with no
  arguments; the analysis will assume that the destructor unlocks whatever the
  constructor locked.

.. cpp:function:: PW_EXCLUSIVE_LOCK_FUNCTION()

  Documents functions that acquire a lock in the body of a function, and do
  not release it.

.. cpp:function:: PW_SHARED_LOCK_FUNCTION()

   Documents functions that acquire a shared (reader) lock in the body of a
   function, and do not release it.

.. cpp:function:: PW_UNLOCK_FUNCTION()

   Documents functions that expect a lock to be held on entry to the function,
   and release it in the body of the function.

.. cpp:function:: PW_EXCLUSIVE_TRYLOCK_FUNCTION(try_success)
.. cpp:function:: PW_SHARED_TRYLOCK_FUNCTION(try_success)

  Documents functions that try to acquire a lock, and return success or failure
  (or a non-boolean value that can be interpreted as a boolean).
  The first argument should be ``true`` for functions that return ``true`` on
  success, or ``false`` for functions that return `false` on success. The second
  argument specifies the lock that is locked on success. If unspecified, this
  lock is assumed to be ``this``.

.. cpp:function:: PW_ASSERT_EXCLUSIVE_LOCK()
.. cpp:function:: PW_ASSERT_SHARED_LOCK()

   Documents functions that dynamically check to see if a lock is held, and fail
   if it is not held.

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
