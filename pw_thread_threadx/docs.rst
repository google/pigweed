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
