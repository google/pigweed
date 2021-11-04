.. _module-pw_sync_threadx:

===============
pw_sync_threadx
===============
This is a set of backends for pw_sync based on ThreadX.

It is possible, if necessary, to use pw_sync_threadx without using the Pigweed
provided pw_chrono_threadx in case the ThreadX time API (``tx_time_get()``)) is
not available (i.e. ``TX_NO_TIMER`` is set). You are responsible for ensuring
that the chrono backend provided has counts which match the ThreadX tick based
API.

--------------------------------
Critical Section Lock Primitives
--------------------------------

Mutex & TimedMutex
==================
The ThreadX backend for the Mutex and TimedMutex use ``TX_MUTEX`` as the
underlying type. It is created using ``tx_mutex_create`` as part of the
constructors and cleaned up using ``tx_mutex_delete`` in the destructors.

InterruptSpinLock
=================
The ThreadX backend for InterruptSpinLock is backed by an ``enum class`` and
two ``UINT`` which permits these objects to detect accidental recursive locking
and unlocking contexts.

This object uses ``tx_interrupt_control`` to enable critical sections. In
addition, ``tx_thread_preemption_change`` is used to prevent accidental thread
context switches while the InterruptSpinLock is held by a thread.

.. Warning::
  This backend does not support SMP yet as there's no internal lock to spin on.

--------------------
Signaling Primitives
--------------------

ThreadNotification & TimedThreadNotification
============================================
An optimized ThreadX backend for the ThreadNotification and
TimedThreadNotification is provided using ``tx_thread_sleep`` and
``tx_thread_wait_abort``. It is backed by a ``TX_THREAD*`` and a ``bool`` which
permits these objects to track the notification value per instance. In addition,
this backend relies on a global singleton InterruptSpinLock to provide thread
safety in a way which is SMP compatible.

Design Notes
------------
Because ThreadX can support SMP systems, we need a lock which permits spinning
to safely mutate the notification value and whether the thread is blocked.
This could be allocated per ThreadNotification instance, however this cost
quickly adds up with a large number of instances. In addition, the critical
sections are reasonably short.

For those reasons, we opted to go with a single global interrupt spin lock. This
is the minimal memory footprint solution, perfect for uniprocessor systems. In
addition, given that we do not expect any serious contention for most of our
SMP users, we believe this is a good trade-off.

On very large SMP systems which heavily rely on ThreadNotifications, one could
consider moving the InterruptSpinLock to the ThreadNotification instance if
contention on this global lock becomes a problem.

BinarySemaphore & CountingSemaphore
===================================
The ThreadX backends for the BinarySemaphore and CountingSemaphore use
``TX_SEMAPHORE`` as the underlying type. It is created using
``tx_semaphore_create`` as part of the constructor and cleaned up using
``tx_semaphore_delete`` in the destructor.
