.. _module-pw_thread_freertos:

==================
pw_thread_freertos
==================
This is a set of backends for pw_thread based on FreeRTOS.

.. contents::
   :local:
   :depth: 1

.. Warning::
  This module is still under construction, the API is not yet stable.

-----------------------
Thread Creation Backend
-----------------------
A backend for ``pw::thread::Thread`` is offered using ``xTaskCreateStatic()``.
Optional dynamic allocation for threads is supported using ``xTaskCreate()``.
Optional joining support is enabled via an ``StaticEventGroup_t`` in each
thread's context.

This backend always permits users to start threads where static contexts are
passed in as an option. As a quick example, a detached thread can be created as
follows:

.. code-block:: cpp

  #include "FreeRTOS.h"
  #include "pw_thread/detached_thread.h"
  #include "pw_thread_freertos/context.h"
  #include "pw_thread_freertos/options.h"

  pw::thread::freertos::StaticContextWithStack<42> example_thread_context;
  void StartExampleThread() {
    pw::thread::DetachedThread(
        pw::thread::freertos::Options()
            .set_name("static_example_thread")
            .set_priority(kFooPriority)
            .set_static_context(example_thread_context),
        example_thread_function)
  }

Alternatively when ``PW_THREAD_FREERTOS_CONFIG_DYNAMIC_ALLOCATION_ENABLED`` is
enabled, dynamic thread allocation can be used. The above example could instead
be done as follows:

.. code-block:: cpp

  #include "FreeRTOS.h"
  #include "pw_thread/detached_thread.h"
  #include "pw_thread_freertos/context.h"
  #include "pw_thread_freertos/options.h"

  void StartExampleThread() {
    pw::thread::DetachedThread(
        pw::thread::freertos::Options()
            .set_name("dyanmic_example_thread")
            .set_priority(kFooPriority)
            .set_stack_size(42),
        example_thread_function)
  }


Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration of
this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_THREAD_FREERTOS_CONFIG_JOINING_ENABLED

  Whether thread joining is enabled. By default this is disabled.

  We suggest only enabling this when thread joining is required to minimize
  the RAM and ROM cost of threads.

  Enabling this grows the RAM footprint of every ``pw::thread::Thread`` as it
  adds a ``StaticEventGroup_t`` to every thread's
  ``pw::thread::freertos::Context``. In addition, there is a minute ROM cost to
  construct and destroy this added object.

  ``PW_THREAD_JOINING_ENABLED`` gets set to this value.

.. c:macro:: PW_THREAD_FREERTOS_CONFIG_DYNAMIC_ALLOCATION_ENABLED

  Whether dynamic allocation for threads (stacks and contexts) is enabled. By
  default this matches the FreeRTOS configuration on whether dynamic
  allocations are enabled. Note that static contexts **must** be provided if
  dynamic allocations are disabled.

.. c:macro:: PW_THREAD_FREERTOS_CONFIG_DEFAULT_STACK_SIZE_WORDS

   The default stack size in words. By default this uses the minimal FreeRTOS
   stack size based on ``configMINIMAL_STACK_SIZE``.

.. c:macro:: PW_THREAD_FREERTOS_CONFIG_DEFAULT_PRIORITY

   The default stack size in words. By default this uses the minimal FreeRTOS
   priority level above the idle priority (``tskIDLE_PRIORITY + 1``).

FreeRTOS Thread Options
=======================
.. cpp:class:: pw::thread::freertos::Options

  .. cpp:function:: set_name(const char* name)

    Sets the name for the FreeRTOS task, note that this will be truncated
    based on ``configMAX_TASK_NAME_LEN``. This is deep copied by FreeRTOS into
    the task's task control block (TCB).

  .. cpp:function:: set_priority(UBaseType_t priority)

    Sets the priority for the FreeRTOS task. This must be a value between
    ``tskIDLE_PRIORITY`` or ``0`` to ``configMAX_PRIORITIES - 1``. Higher
    priority values have a higher priority.

  .. cpp:function:: set_stack_size(size_t size_words)

    Set the stack size in words for a dynamically thread.

    This is only available if
    ``PW_THREAD_FREERTOS_CONFIG_DYNAMIC_ALLOCATION_ENABLED`` is enabled.

    Precondition: size_words must be >= ``configMINIMAL_STACK_SIZE``

  .. cpp:function:: set_static_context(pw::thread::freertos::Context& context)

    Set the pre-allocated context (all memory needed to run a thread). The
    ``StaticContext`` can either be constructed with an externally provided
    ``std::span<StackType_t>`` stack or the templated form of
    ``StaticContextWithStack<kStackSizeWords>`` can be used.


-----------------------------
Thread Identification Backend
-----------------------------
A backend for ``pw::thread::Id`` and ``pw::thread::get_id()`` is offerred using
``xTaskGetCurrentTaskHandle()``. It uses ``DASSERT`` to ensure that it is not
invoked from interrupt context and if possible that the scheduler has started
via ``xTaskGetSchedulerState()``.

--------------------
Thread Sleep Backend
--------------------
A backend for ``pw::thread::sleep_for()`` and ``pw::thread::sleep_until()`` is
offerred using ``vTaskDelay()`` if the duration is at least one tick, else
``taskYIELD()`` is used. It uses ``pw::this_thread::get_id() != thread::Id()``
to ensure it invoked only from a thread.

--------------------
Thread Yield Backend
--------------------
A backend for ``pw::thread::yield()`` is offered using via ``taskYIELD()``.
It uses ``pw::this_thread::get_id() != thread::Id()`` to ensure it invoked only
from a thread.
