.. _module-pw_thread_zephyr:

================
pw_thread_zephyr
================
This is a set of backends for pw_thread based on the Zephyr RTOS.

.. Warning::
  This module is still under construction, the API is not yet stable and
  documentation is incomplete.

-----------------------
Thread Creation Backend
-----------------------
A backend for ``pw::thread::Thread`` is offered using ``k_thread_create()``.
This backend only supports threads with static contexts (which are passed via
Options).
All created threads support joining and detaching.
To enable this backend, add ``CONFIG_PIGWEED_THREAD=y`` to the Zephyr
project`s configuration.

.. code-block:: cpp

   #include "pw_thread/detached_thread.h"
   #include "pw_thread_zephyr/config.h"
   #include "pw_thread_zephyr/context.h"
   #include "pw_thread_zephyr/options.h"

   constexpr int kFooPriority =
       pw::thread::zephyr::config::kDefaultPriority;
   constexpr size_t kFooStackSizeBytes = 512;

   pw::thread::zephyr::StaticContextWithStack<kFooStackSizeBytes>
       example_thread_context;
   void StartExampleThread() {
     pw::thread::DetachedThread(
         pw::thread::zephyr::Options(example_thread_context)
             .set_priority(kFooPriority),
         example_thread_function, example_arg);
   }

--------------------
Thread Sleep Backend
--------------------
A backend for ``pw::thread::sleep_for()`` and ``pw::thread::sleep_until()``.
To enable this backend, add ``CONFIG_PIGWEED_THREAD_SLEEP=y``
to the Zephyr project`s configuration.
