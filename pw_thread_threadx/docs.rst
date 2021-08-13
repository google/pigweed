.. _module-pw_thread_threadx:

=================
pw_thread_threadx
=================
This is a set of backends for pw_thread based on ThreadX.

.. Warning::
  This module is still under construction, the API is not yet stable.

.. list-table::

  * - :ref:`module-pw_thread` Facade
    - Backend Target
    - Description
  * - ``pw_thread:id``
    - ``pw_thread_threadx:id``
    - Thread identification.
  * - ``pw_thread:yield``
    - ``pw_thread_threadx:yield``
    - Thread scheduler yielding.
  * - ``pw_thread:sleep``
    - ``pw_thread_threadx:sleep``
    - Thread scheduler sleeping.
  * - ``pw_thread:thread``
    - ``pw_thread_threadx:thread``
    - Thread creation.

Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration of
this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_THREAD_THREADX_CONFIG_JOINING_ENABLED

  Whether thread joining is enabled. By default this is disabled.

  We suggest only enabling this when thread joining is required to minimize
  the RAM and ROM cost of threads.

  Enabling this grows the RAM footprint of every pw::thread::Thread as it adds
  a TX_EVENT_FLAGS_GROUP to every thread's pw::thread::threadx::Context. In
  addition, there is a minute ROM cost to construct and destroy this added
  object.

  PW_THREAD_JOINING_ENABLED gets set to this value.

.. c:macro:: PW_THREAD_THREADX_CONFIG_DEFAULT_STACK_SIZE_WORDS

  The default stack size in words. By default this uses the minimal ThreadX
  stack size.

.. c:macro:: PW_THREAD_THREADX_CONFIG_MAX_THREAD_NAME_LEN

  The maximum length of a thread's name, not including null termination. By
  default this is arbitrarily set to 15. This results in an array of characters
  which is this length + 1 bytes in every pw::thread::Thread's context.

.. c:macro:: PW_THREAD_THREADX_CONFIG_DEFAULT_TIME_SLICE_INTERVAL

  The round robin time slice tick interval for threads at the same priority.
  By default this is disabled as not all ports support this, using a value of 0
  ticks.

.. c:macro:: PW_THREAD_THREADX_CONFIG_MIN_PRIORITY

  The minimum priority level, this is normally based on the number of priority
  levels.

.. c:macro:: PW_THREAD_THREADX_CONFIG_DEFAULT_PRIORITY

  The default priority level. By default this uses the minimal ThreadX
  priority level, given that 0 is the highest priority.

.. c:macro:: PW_THREAD_THREADX_CONFIG_LOG_LEVEL

  The log level to use for this module. Logs below this level are omitted.

---------
utilities
---------
In cases where an operation must be performed for every thread,
``ForEachThread()`` can be used to iterate over all the created thread TCBs.
Note that it's only safe to use this while the scheduler is disabled.

An ``Aborted`` error status is returned if the provided callback returns
``false`` to request an early termination of thread iteration.

Return values
=============

* ``Aborted``: The callback requested an early-termination of thread iteration.
* ``OkStatus``: The callback has been successfully run with every thread.

--------------------
Snapshot integration
--------------------
This ``pw_thread`` backend provides helper functions that capture ThreadX thread
state to a ``pw::thread::Thread`` proto.

SnapshotThread()/SnapshotThreads()
==================================
``SnapshotThread()`` captures the thread name, state, and stack information for
the provided ThreadX TCB to a ``pw::thread::Thread`` protobuf encoder. To ensure
the most up-to-date information is captured, the stack pointer for the currently
running thread must be provided for cases where the running thread is being
captured. For ARM Cortex-M CPUs, you can do something like this:

.. Code:: cpp

  // Capture PSP.
  void* stack_ptr = 0;
  asm volatile("mrs %0, psp\n" : "=r"(stack_ptr));
  pw::thread::ProcessThreadStackCallback cb =
      [](pw::thread::Thread::StreamEncoder& encoder,
         pw::ConstByteSpan stack) -> pw::Status {
    return encoder.WriteRawStack(stack);
  };
  pw::thread::threadx::SnapshotThread(my_thread, stack_ptr,
                                      snapshot_encoder, cb);

``SnapshotThreads()`` wraps the singular thread capture to instead captures
all created threads to a ``pw::thread::SnapshotThreadInfo`` message. This proto
message overlays a snapshot, so it is safe to static cast a
``pw::snapshot::Snapshot::StreamEncoder`` to a
``pw::thread::SnapshotThreadInfo::StreamEncoder`` when calling this function.
