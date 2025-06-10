.. _module-pw_thread_zephyr:

================
pw_thread_zephyr
================
.. pigweed-module::
   :name: pw_thread_zephyr

This is a set of backends for pw_thread based on the Zephyr RTOS.

---------------
Thread Creation
---------------
``pw_thread_zephyr`` backend fully supports the thread creation API. See
:ref:`Thread creation section <module-pw_thread-thread-creation>` for details.

--------------------
Thread Sleep Backend
--------------------
A backend for ``pw::thread::sleep_for()`` and ``pw::thread::sleep_until()``.
To enable this backend, add ``CONFIG_PIGWEED_THREAD_SLEEP=y``
to the Zephyr project`s configuration.

--------------------
Thread Yield Backend
--------------------
A backend for ``pw::this_thread::yield()``.
To enable this backend, add ``CONFIG_PIGWEED_THREAD_YIELD=y``
to the Zephyr project`s configuration.
