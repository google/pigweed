.. _module-pw_chrono_freertos:

------------------
pw_chrono_freertos
------------------
``pw_chrono_freertos`` is a collection of ``pw_chrono`` backends that are
implemented using FreeRTOS.

.. warning::
  This module is under construction, not ready for use, and the documentation
  is incomplete.

SystemClock backend
-------------------
The FreeRTOS based ``system_clock`` backend implements the
``pw_chrono:system_clock`` facade by using ``xTaskGetTickCountFromISR()`` and
``xTaskGetTickCount()`` based on the current context. An InterruptSpinLock is
used to manage overflows in a thread and interrupt safe manner to produce a
signed 64 bit timestamp.

The ``SystemClock::now()`` must be used more than once per overflow of the
native FreeRTOS ``xTaskGetTickCount*()`` overflow. Note that this duration may
vary if ``portSUPPRESS_TICKS_AND_SLEEP()``, ``vTaskStepTick()``, and/or
``xTaskCatchUpTicks()`` are used.

Build targets
-------------
The GN build for ``pw_chrono_freertos`` has one target: ``system_clock``.
The ``system_clock`` target provides the
``pw_chrono_backend/system_clock_config.h`` and ``pw_chrono_freertos/config.h``
headers and the backend for the ``pw_chrono:system_clock``.
