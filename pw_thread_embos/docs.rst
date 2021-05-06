.. _module-pw_thread_embos:

===============
pw_thread_embos
===============
This is a set of backends for pw_thread based on embOS v4.

.. contents::
   :local:
   :depth: 1

.. Warning::
  This module is still under construction, the API is not yet stable.

-----------------------
Thread Creation Backend
-----------------------
A backend or ``pw::thread::Thread`` is offered using ``OS_CreateTaskEx()``.
Optional joining support is enabled via an ``OS_EVENT`` in each thread's
context.

This backend permits users to start threads where contexts must be explicitly
allocated and passed in as an option. As a quick example, a detached thread
can be created as follows:

.. code-block:: cpp

  #include "pw_thread/detached_thread.h"
  #include "pw_thread_embos/context.h"
  #include "pw_thread_embos/options.h"
  #include "RTOS.h"  // For the embOS types.

  pw::thread::embos::ContextWithStack<42> example_thread_context;
  void StartExampleThread() {
    pw::thread::DetachedThread(
        pw::thread::embos::Options()
            .set_name("static_example_thread")
            .set_priority(kFooPriority)
            .set_time_slice_interval(kFooTimeSliceInterval)
            .set_context(example_thread_context),
        example_thread_function)
  }


Module Configuration Options
============================
The following configurations can be adjusted via compile-time configuration of
this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_THREAD_EMBOS_CONFIG_JOINING_ENABLED

  Whether thread joining is enabled. By default this is disabled.

  We suggest only enabling this when thread joining is required to minimize
  the RAM and ROM cost of threads.

  Enabling this grows the RAM footprint of every pw::thread::Thread as it adds
  an OS_EVENT to every thread's pw::thread::embos::Context. In addition, there
  is a minute ROM cost to construct and destroy this added object.

  PW_THREAD_JOINING_ENABLED gets set to this value.

.. c:macro:: PW_THREAD_EMBOS_CONFIG_MINIMUM_STACK_SIZE_WORDS

  The minimum stack size in words. By default this uses Segger's recommendation
  of 68 bytes.

.. c:macro:: PW_THREAD_EMBOS_CONFIG_DEFAULT_STACK_SIZE_WORDS

  The default stack size in words. By default this uses Segger's recommendation
  of 256 bytes to start.

.. c:macro:: PW_THREAD_EMBOS_CONFIG_MAX_THREAD_NAME_LEN

  The maximum length of a thread's name, not including null termination. By
  default this is arbitrarily set to 15. This results in an array of characters
  which is this length + 1 bytes in every ``pw::thread::Thread``'s context.

.. c:macro:: PW_THREAD_EMBOS_CONFIG_MIN_PRIORITY

  The minimum priority level, this is normally 1, since 0 is not a valid
  priority level.

.. c:macro:: PW_THREAD_EMBOS_CONFIG_DEFAULT_PRIORITY

  The default priority level. By default this uses the minimal embOS priority.

.. c:macro:: PW_THREAD_EMBOS_CONFIG_DEFAULT_TIME_SLICE_INTERVAL

  The round robin time slice tick interval for threads at the same priority.
  By default this is set to 2 ticks based on the embOS default.


embOS Thread Options
====================
.. cpp:class:: pw::thread::embos::Options

  .. cpp:function:: set_name(const char* name)

    Sets the name for the embOS task, this is optional.
    Note that this will be deep copied into the context and may be truncated
    based on ``PW_THREAD_EMBOS_CONFIG_MAX_THREAD_NAME_LEN``.

  .. cpp:function:: set_priority(OS_PRIO priority)

    Sets the priority for the embOS task. Higher values are higher priority,
    see embOS OS_CreateTaskEx for more detail.
    Precondition: This must be >= ``PW_THREAD_EMBOS_CONFIG_MIN_PRIORITY``.

  .. cpp:function:: set_time_slice_interval(OS_UINT time_slice_interval)

    Sets the number of ticks this thread is allowed to run before other ready
    threads of the same priority are given a chance to run.

    A value of 0 disables time-slicing of this thread.

    Precondition: This must be <= 255 ticks.

  .. cpp:function:: set_context(pw::thread::embos::Context& context)

    Set the pre-allocated context (all memory needed to run a thread). Note that
    this is required for this thread creation backend! The ``Context`` can
    either be constructed with an externally provided ``std::span<OS_UINT>``
    stack or the templated form of ``ContextWithStack<kStackSizeWords>`` can
    be used.


-----------------------------
Thread Identification Backend
-----------------------------
A backend for ``pw::thread::Id`` and ``pw::thread::get_id()`` is offerred using
``OS_GetTaskID()``. It uses ``DASSERT`` to ensure that the scheduler has started
via ``OS_IsRunning()``.

--------------------
Thread Sleep Backend
--------------------
A backend for ``pw::thread::sleep_for()`` and ``pw::thread::sleep_until()`` is
offerred using ``OS_Delay()`` if the duration is at least one tick, else
``OS_Yield()`` is used. It uses ``pw::this_thread::get_id() != thread::Id()`` to
ensure it invoked only from a thread.

--------------------
Thread Yield Backend
--------------------
A backend for ``pw::thread::yield()`` is offered using via ``OS_Yield()``.
It uses ``pw::this_thread::get_id() != thread::Id()`` to ensure it invoked only
from a thread.
