.. _module-pw_sync_freertos:

================
pw_sync_freertos
================
This is a set of backends for pw_sync based on FreeRTOS.

--------------------------------
Critical Section Lock Primitives
--------------------------------

Mutex & TimedMutex
==================
The FreeRTOS backend for the Mutex and TimedMutex use ``StaticSemaphore_t`` as
the underlying type. It is created using ``xSemaphoreCreateMutexStatic`` as part
of the constructors and cleaned up using ``vSemaphoreDelete`` in the
destructors.

.. Note::
  Static allocation support is required in your FreeRTOS configuration, i.e.
  ``configSUPPORT_STATIC_ALLOCATION == 1``.

InterruptSpinLock
=================
The FreeRTOS backend for InterruptSpinLock is backed by ``UBaseType_t`` and a
``bool`` which permits these objects to stash the saved interrupt mask and to
detect accidental recursive locking.

This object uses ``taskENTER_CRITICAL_FROM_ISR`` and
``taskEXIT_CRITICAL_FROM_ISR`` from interrupt contexts, and
``taskENTER_CRITICAL`` and ``taskEXIT_CRITICAL`` in all other contexts.
``vTaskSuspendAll`` and ``xTaskResumeAll`` are additionally used within
lock/unlock respectively when called from task context in the scheduler-enabled
state.

.. Note::
  Scheduler State API support is required in your FreeRTOS Configuration, i.e.
  ``INCLUDE_xTaskGetSchedulerState == 1``.

.. warning::
  ``taskENTER_CRITICAL_FROM_ISR`` only disables interrupts with priority at or
  below ``configMAX_SYSCALL_INTERRUPT_PRIORITY``. Therefore, it is unsafe to
  use InterruptSpinLock from higher-priority interrupts, even if they are not
  non-maskable interrupts. This is consistent with the rest of the FreeRTOS
  APIs, see the `FreeRTOS kernel interrupt priority documentation
  <https://www.freertos.org/a00110.html#kernel_priority>`_ for more details.

--------------------
Signaling Primitives
--------------------

ThreadNotification & TimedThreadNotification
============================================
An optimized FreeRTOS backend for the ThreadNotification and
TimedThreadNotification is provided using Task Notifications. It is backed by a
``TaskHandle_t`` and a ``bool`` which permits these objects to track the
notification value outside of the task's TCB (AKA FreeRTOS Task Notification
State and Value).

.. Warning::
  By default this backend uses the task notification at index 0, just like
  FreeRTOS Stream and Message Buffers. If you want to maintain the state of a
  task notification across blocking acquiring calls to ThreadNotifications, then
  you must do one of the following:

  1. Adjust ``PW_SYNC_FREERTOS_CONFIG_THREAD_NOTIFICATION_INDEX`` to an index
     which does not collide with existing incompatible use.
  2. Migrate your existing use of task notifications away from index 0.
  3. Do not use this optimized backend and instead use the binary semaphore
     backends for ThreadNotifications
     (``pw_sync:binary_semaphore_thread_notification_backend``).

  You are using any of the following Task Notification APIs, it means you are
  using notification indices:

  - ``xTaskNotify`` / ``xTaskNotifyIndexed``
  - ``xTaskNotifyFromISR`` / ``xTaskNotifyIndexedFromISR``
  - ``xTaskNotifyGive`` / ``xTaskNotifyGiveIndexed``
  - ``xTaskNotifyGiveFromISR`` / ``xTaskNotifyGiveIndexedFromISR``
  - ``xTaskNotifyAndQuery`` / ``xTaskNotifyAndQueryIndexed``
  - ``xTaskNotifyAndQueryFromISR`` / ``xTaskNotifyAndQueryIndexedFromISR``
  - ``ulTaskNotifyTake`` / ``ulTaskNotifyTakeIndexed``
  - ``xTaskNotifyWait`` / ``xTaskNotifyWaitIndexed``
  - ``xTaskNotifyStateClear`` / ``xTaskNotifyStateClearIndexed``
  - ``ulTaskNotifyValueClear`` / ``ulTaskNotifyValueClearIndexed``

  APIs without ``Indexed`` in the name use index 0 implicitly.

  Prior to FreeRTOS V10.4.0 each task had a single "notification index", and all
  task notification API functions operated on that implicit index of 0.

This backend is compatible with sharing the notification index
with native FreeRTOS
`Stream and Message Buffers <https://www.freertos.org/RTOS-task-notifications.html>`_
at index 0.

Just like FreeRTOS Stream and Message Buffers, this backend uses the task
notification index only within callsites where the task must block until a
notification is received or a timeout occurs. The notification index's state is
always cleaned up before returning. The notification index is never used when
the acquiring task is not going to block.

.. Note::
  Task notification support is required in your FreeRTOS configuration, i.e.
  ``configUSE_TASK_NOTIFICATIONS == 1``.

Design Notes
------------
You may ask, why are Task Notifications used at all given the risk associated
with global notification index allocations? It turns out there's no other
lightweight mechanism to unblock a task in FreeRTOS.

Task suspension (i.e. ``vTaskSuspend``, ``vTaskResume``, &
``vTaskResumeFromISR``) seems like a good fit, however ``xTaskResumeAll`` does
not participate in reference counting and will wake up all suspended tasks
whether you want it to or not.

Lastly, there's also ``xTaskAbortDelay`` but there is no interrupt safe
equivalent of this API. Note that it uses ``vTaskSuspendAll`` internally for
the critical section which is not interrupt safe. If in the future an interrupt
safe version of this API is offerred, then this would be a great alternative!

Lastly, we want to briefly explain how Task Notifications actually work in
FreeRTOS to show why you cannot directly share notification indeces even if the
bits used in the ``ulNotifiedValue`` are unique. This is a very common source of
bugs when using FreeRTOS and partially why Pigweed does not recommend using the
native Task Notification APIs directly.

FreeRTOS Task Notifications use a task's TCB's ``ucNotifyState`` to capture the
notification state even when the task is not blocked. This state transitions
``taskNOT_WAITING_NOTIFICATION`` to ``task_NOTIFICATION_RECEIVED`` if the task
ever notified. This notification state is used to determine whether the next
task notification wait call should block, irrespective of the notification
value.

In order to enable this optimized backend, native task notifications are only
used when the task needs to block. If a timeout occurs the task unregisters for
notifications and clears the notification state before returning. This exact
mechanism is used by FreeRTOS internally for their Stream and Message Buffer
implementations.

One other thing to note is that FreeRTOS has undocumented side effects between
``vTaskSuspend`` and ``xTaskNotifyWait``. If a thread is suspended via
``vTaskSuspend`` while blocked on ``xTaskNotifyWait``, the wait is aborted
regardless of the timeout (even if the request was indefinite) and the thread
is resumed whenever ``vTaskResume`` is invoked.

BinarySemaphore
===============
The FreeRTOS backend for the BinarySemaphore uses ``StaticSemaphore_t`` as the
underlying type. It is created using ``xSemaphoreCreateBinaryStatic`` as part
of the constructor and cleaned up using ``vSemaphoreDelete`` in the destructor.

.. Note::
  Static allocation support is required in your FreeRTOS configuration, i.e.
  ``configSUPPORT_STATIC_ALLOCATION == 1``.

CountingSemaphore
=================
The FreeRTOS backend for the CountingSemaphore uses ``StaticSemaphore_t`` as the
underlying type. It is created using ``xSemaphoreCreateCountingStatic`` as part
of the constructor and cleaned up using ``vSemaphoreDelete`` in the destructor.

.. Note::
  Counting semaphore support is required in your FreeRTOS configuration, i.e.
  ``configUSE_COUNTING_SEMAPHORES == 1``.
.. Note::
  Static allocation support is required in your FreeRTOS configuration, i.e.
  ``configSUPPORT_STATIC_ALLOCATION == 1``.


