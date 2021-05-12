.. _docs-os_abstraction_layers:

=====================
OS Abstraction Layers
=====================
Pigweed’s operating system abstraction layers are configurable building blocks.
They are designed to be lightweight, portable, and easy to use while giving
users full control and configurability.

Although we primarily target smaller-footprint MMU-less 32-bit microcontrollers,
the OS abstraction layers are written to work on everything from single-core
bare metal low end microcontrollers to asymmetric multiprocessing (AMP) and
symmetric multiprocessing (SMP) embedded systems using Real Time Operating
Systems (RTOS). They even fully work on your developer workstation on Linux,
Windows, or MacOS!

.. list-table::

  * - **Environment**
    - **Status**
  * - FreeRTOS
    - Supported
  * - ThreadX
    - Supported
  * - embOS
    - *In Progress*
  * - STL
    - Supported
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned
  * - Baremetal
    - *In Progress*

.. contents::
   :local:
   :depth: 1

-------------
OS Primitives
-------------
Pigweed's OS abstraction layers are divided by the functional grouping of the
primitives. Many of our APIs are similar or nearly identical to C++'s Standard
Template Library (STL) with the notable exception that we do not support
exceptions. We opted to follow the STL's APIs partially because they are
relatively well thought out and many developers are already familiar with them,
but also because this means they are compatible with existing helpers in the STL
which can then be further leveraged.

---------------------------
pw_chrono - Time Primitives
---------------------------
The :ref:`module-pw_chrono` module provides the building blocks for expressing
durations, timestamps, and acquiring the current time. This in turn is used by
other modules, including  :ref:`module-pw_sync` and :ref:`module-pw_thread` as
the basis for any time bound APIs (i.e. with timeouts and/or deadlines). Note
that this module is optional and bare metal targets may opt not to use this.

.. list-table::

  * - **Supported On**
    - **SystemClock**
  * - FreeRTOS
    - :ref:`module-pw_chrono_freertos`
  * - ThreadX
    - :ref:`module-pw_chrono_threadx`
  * - embOS
    - :ref:`module-pw_chrono_embos`
  * - STL
    - :ref:`module-pw_chrono_stl`
  * - Zephyr
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned
  * - Baremetal
    - Planned


SystemClock
===========
For RTOS and HAL interactions, we provide a ``pw::chrono::SystemClock`` facade
which provides 64 bit timestamps and duration support along with a C API. For
C++ there is an optional virtual wrapper, ``pw::chrono::VirtualSystemClock``,
around the singleton clock facade to enable dependency injection.

.. code-block:: cpp

  #include <chrono>

  #include "pw_thread/sleep.h"

  using namespace std::literals::chrono_literals;

  void ThisSleeps() {
    pw::thread::sleep_for(42ms);
  }

Unlike the STL's time bound templated APIs which are not specific to a
particular clock, Pigweed's time bound APIs are strongly typed to use the
``pw::chrono::SystemClock``'s ``duration`` and ``time_points`` directly.

.. code-block:: cpp

  #include "pw_chrono/system_clock.h"

  bool HasThisPointInTimePassed(const SystemClock::time_point timestamp) {
    return SystemClock::now() > timestamp;
  }

------------------------------------
pw_sync - Synchronization Primitives
------------------------------------
The :ref:`module-pw_sync` provides the building blocks for synchronizing between
threads and/or interrupts through signaling primitives and critical section lock
primitives.

Critical Section Lock Primitives
================================
Pigweed's locks support Clang's thread safety lock annotations and the STL's
RAII helpers.

.. list-table::

  * - **Supported On**
    - **Mutex**
    - **TimedMutex**
    - **InterruptSpinLock**
  * - FreeRTOS
    - :ref:`module-pw_sync_freertos`
    - :ref:`module-pw_sync_freertos`
    - :ref:`module-pw_sync_freertos`
  * - ThreadX
    - :ref:`module-pw_sync_threadx`
    - :ref:`module-pw_sync_threadx`
    - :ref:`module-pw_sync_threadx`
  * - embOS
    - :ref:`module-pw_sync_embos`
    - :ref:`module-pw_sync_embos`
    - :ref:`module-pw_sync_embos`
  * - STL
    - :ref:`module-pw_sync_stl`
    - :ref:`module-pw_sync_stl`
    - :ref:`module-pw_sync_stl`
  * - Zephyr
    - Planned
    - Planned
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned
    - Planned
    - Planned
  * - Baremetal
    - Planned, not ready for use
    - ✗
    - Planned, not ready for use


Thread Safe Mutex
-----------------
The ``pw::sync::Mutex`` protects shared data from being simultaneously accessed
by multiple threads. Optionally, the ``pw::sync::TimedMutex`` can be used as an
extension with timeout and deadline based semantics.

.. code-block:: cpp

  #include <mutex>

  #include "pw_sync/mutex.h"

  pw::sync::Mutex mutex;

  void ThreadSafeCriticalSection() {
    std::lock_guard lock(mutex);
    NotThreadSafeCriticalSection();
  }

Interrupt Safe InterruptSpinLock
--------------------------------
The ``pw::sync::InterruptSpinLock`` protects shared data from being
simultaneously accessed by multiple threads and/or interrupts as a targeted
global lock, with the exception of Non-Maskable Interrupts (NMIs). Unlike global
interrupt locks, this also works safely and efficiently on SMP systems.

.. code-block:: cpp

  #include <mutex>

  #include "pw_sync/interrupt_spin_lock.h"

  pw::sync::InterruptSpinLock interrupt_spin_lock;

  void InterruptSafeCriticalSection() {
    std::lock_guard lock(interrupt_spin_lock);
    NotThreadSafeCriticalSection();
  }

Signaling Primitives
====================
Native signaling primitives tend to vary more compared to critical section locks
across different platforms. For example, although common signaling primitives
like semaphores are in most if not all RTOSes and even POSIX, it was not in the
STL before C++20. Likewise many C++ developers are surprised that conditional
variables tend to not be natively supported on RTOSes. Although you can usually
build any signaling primitive based on other native signaling primitives,
this may come with non-trivial added overhead in ROM, RAM, and execution
efficiency.

For this reason, Pigweed intends to provide some simpler signaling primitives
which exist to solve a narrow programming need but can be implemented as
efficiently as possible for the platform that it is used on. This simpler but
highly portable class of signaling primitives is intended to ensure that a
portability efficiency tradeoff does not have to be made up front.

.. list-table::

  * - **Supported On**
    - **ThreadNotification**
    - **TimedThreadNotification**
    - **CountingSemaphore**
    - **BinarySemaphore**
  * - FreeRTOS
    - :ref:`module-pw_sync_freertos`
    - :ref:`module-pw_sync_freertos`
    - :ref:`module-pw_sync_freertos`
    - :ref:`module-pw_sync_freertos`
  * - ThreadX
    - :ref:`module-pw_sync_threadx`
    - :ref:`module-pw_sync_threadx`
    - :ref:`module-pw_sync_threadx`
    - :ref:`module-pw_sync_threadx`
  * - embOS
    - :ref:`module-pw_sync_embos`
    - :ref:`module-pw_sync_embos`
    - :ref:`module-pw_sync_embos`
    - :ref:`module-pw_sync_embos`
  * - STL
    - :ref:`module-pw_sync_stl`
    - :ref:`module-pw_sync_stl`
    - :ref:`module-pw_sync_stl`
    - :ref:`module-pw_sync_stl`
  * - Zephyr
    - Planned
    - Planned
    - Planned
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned
    - Planned
    - Planned
    - Planned
  * - Baremetal
    - Planned
    - ✗
    - TBD
    - TBD

Thread Notification
-------------------
Pigweed intends to provide the ``pw::sync::ThreadNotification`` and
``pw::sync::TimedThreadNotification`` facades which permit a singler consumer to
block until an event occurs. This should be backed by the most efficient native
primitive for a target, regardless of whether that is a semaphore, event flag
group, condition variable, or direct task notification with a critical section
something else.

Counting Semaphore
------------------
The ``pw::sync::CountingSemaphore`` is a synchronization primitive that can be
used for counting events and/or resource management where receiver(s) can block
on acquire until notifier(s) signal by invoking release.

.. code-block:: cpp

  #include "pw_sync/counting_semaphore.h"

  pw::sync::CountingSemaphore event_semaphore;

  void NotifyEventOccurred() {
    event_semaphore.release();
  }

  void HandleEventsForever() {
    while (true) {
      event_semaphore.acquire();
      HandleEvent();
    }
  }

Binary Semaphore
----------------
The ``pw::sync::BinarySemaphore`` is a specialization of the counting semaphore
with an arbitrary token limit of 1, meaning it's either full or empty.

.. code-block:: cpp

  #include "pw_sync/binary_semaphore.h"

  pw::sync::BinarySemaphore do_foo_semaphore;

  void NotifyResultReady() {
    result_ready_semaphore.release();
  }

  void BlockUntilResultReady() {
    result_ready_semaphore.acquire();
  }

--------------------------------
pw_thread - Threading Primitives
--------------------------------
The :ref:`module-pw_thread` module provides the building blocks for creating and
using threads including yielding and sleeping.

.. list-table::

  * - **Supported On**
    - **Thread Creation**
    - **Thread Id/Sleep/Yield**
  * - FreeRTOS
    - :ref:`module-pw_sync_freertos`
    - :ref:`module-pw_sync_freertos`
  * - ThreadX
    - :ref:`module-pw_sync_threadx`
    - :ref:`module-pw_sync_threadx`
  * - embOS
    - Under Development
    - :ref:`module-pw_sync_embos`
  * - STL
    - :ref:`module-pw_sync_stl`
    - :ref:`module-pw_sync_stl`
  * - Zephyr
    - Planned
    - Planned
  * - CMSIS-RTOS API v2 & RTX5
    - Planned
    - Planned
  * - Baremetal
    - ✗
    - ✗

Thread Creation
===============
The ``pw::thread::Thread``’s API is C++11 STL ``std::thread`` like. Unlike
``std::thread``, the Pigweed's API requires ``pw::thread::Options`` as an
argument for creating a thread. This is used to give the user full control over
the native OS's threading options without getting in your way.

.. code-block:: cpp

  #include "pw_thread/detached_thread.h"
  #include "pw_thread_freertos/context.h"
  #include "pw_thread_freertos/options.h"

  pw::thread::freertos::ContextWithStack<42> example_thread_context;

  void StartDetachedExampleThread() {
     pw::thread::DetachedThread(
       pw::thread::freertos::Options()
           .set_name("static_example_thread")
           .set_priority(kFooPriority)
           .set_static_context(example_thread_context),
       example_thread_function);
  }

Controlling the current thread
==============================
Beyond thread creation, Pigweed offers support for sleeping, identifying, and
yielding the current thread.

.. code-block:: cpp

  #include "pw_thread/yield.h"

  void CooperativeBusyLooper() {
    while (true) {
      DoChunkOfWork();
      pw::this_thread::yield();
    }
  }

--------------------------------------------
Execution Contexts & Thread-Safety API Model
--------------------------------------------
The explosion of real contexts is too large for Pigweed to fully cover in a way
that provides value. First there are many more contexts than just threads and
IRQ handlers on microcontrollers, there are many more meta contexts like
non-blocking thread callbacks which may have scheduling and/or interrupts masked
to some degree. On top of this some environments like in userspace may not even
have interrupts and instead deal with signals. Instead we use the following
simplified execution thread-safety model which our APIs should be ported to
support regardless of the real contexts they are executed in:

**Thread Safe APIs** - These APIs are safe to use in any execution context where
one can use blocking or yielding APIs such as sleeping, blocking on a mutex
waiting on a semaphore.

**Interrupt (IRQ) Safe APIs** - These APIs can be used in any execution context
which cannot use blocking and yielding APIs. These APIs must protect themselves
from preemption from maskable interrupts, etc. This includes critical section
thread contexts in addition to "real" interrupt contexts. Our definition
explicitly excludes any interrupts which are not masked when holding a SpinLock,
those are all considered non-maskable interrupts. An interrupt safe API may
always be safely used in a context which permits thread safe APIs.

**Non-Maskable Interrupt (NMI) Safe APIs** - Like the Interrupt Safe APIs, these
can be used in any execution context which cannot use blocking or yielding APIs.
In addition, these may be used by interrupts which are not masked when for
example holding a SpinLock like CPU exceptions or C++/POSIX signals. These tend
to come with significant overhead and restrictions compared to regular interrupt
safe APIs as they cannot rely on critical sections for implementations, instead
only atomic signaling can be used. An interrupt safe API may always be safely
used in a context which permits interrupt safe and thread safe APIs.

Instead of going with context specific APIs, e.g. FreeRTOS's ``*FromISR()``
APIs, Pigweed opted to go with the merged (context agnostic) API which validates
the context requirements through ``DASSERT`` and ``DCHECK`` in the backends
(user configurable). We did this primarily for two reasons. The explosion of
real contexts is too large for Pigweed to fully cover as mentioned above,
meaning there would likely have to be some context aware multiplexing with our
simplified thread safety model split APIs. Second, we would recommend a
``DHCECK`` to enforce context requirements regardless, so we've opted with a
simplest API which also happens to match both the C++'s STL and Google's Abseil
relatively closely.

---------------------------------------------------
Construction Requirements & Initialization Paradigm
---------------------------------------------------

**TL;DR: Pigweed OS primitives are initialized through C++ construction.**

We have chosen to go with a model which initializes the synchronization
primitive during C++ object construction. This means that there is a requirement
in order for static instantiation to be safe that the user ensures that any
necessary kernel and/or platform initialization is done before the global static
constructors are run which would include construction of the C++ synchronization
primitives.

In addition this model for now assumes that Pigweed code will always be used to
construct synchronization primitives used with Pigweed modules. Note that with
this model the backend provider can decide if they want to statically
preallocate space for the primitives or rely on dynamic allocation strategies.
If we discover at a later point that this is not sufficiently portable than we
can either produce an optional constructor that takes in a reference to an
existing native synchronization type and wastes a little bit RAM or we can
refactor the existing class into two layers where one is a StaticMutex for
example and the other is a Mutex which only holds a handle to the native mutex
type. This would then permit users who cannot construct their synchronization
primitives to skip the optional static layer.

Kernel / Platform Initialization Before C++ Global Static Constructors
======================================================================
What is this kernel and/or platform initialization that must be done first?

It's not uncommon for an RTOS to require some initialization functions to be
invoked before more of its API can be safely used. For example for CMSIS RTOSv2
``osKernelInitialize()`` must be invoked before anything but two basic getters
are called. Similarly, Segger's embOS requires ``OS_Init()`` to be invoked first
before any other embOS API.

.. Note::
  To get around this one should invoke these initialization functions earlier
  and/or delay the static C++ constructors to meet this ordering requirement. As
  an example if you were using :ref:`module-pw_boot_armv7m`, then
  ``pw_boot_PreStaticConstructorInit()`` would be a great place to invoke kernel
  initialization.

-------
Roadmap
-------
Pigweed is still actively expanding and improving its OS Abstraction Layers.
That being said, the following concrete areas are being worked on and can be
expected to land at some point in the future:

1. Thread creation support for embOS is in progress.
2. We'd like to offer a system clock based timer abstraction facade which can be
   used on either an RTOS or a hardware timer.
3. We are evaluating a less-portable but very useful portability facade for
   event flags / groups. This would make it even easier to ensure all firmware
   can be fully executed on the host.
4. Cooperative cancellation thread joining along with a ``std::jhtread`` like
   wrapper is in progress.
5. We'd like to add support for queues, message queues, and similar channel
   abstractions which also support interprocessor communication in a transparent
   manner.
6. We're interested in supporting asynchronous worker queues and worker queue
   pools.
7. Migrate HAL and similar APIs to use deadlines for the backend virtual
   interfaces to permit a smaller vtable which supports both public timeout and
   deadline semantics.
8. Baremetal support is partially in place today, but it's not ready for use.
9. Most of our APIs today are focused around synchronous blocking APIs, however
   we would love to extend this to include asynchronous APIs.
