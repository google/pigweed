.. _module-pw_chrono_embos:

---------------
pw_chrono_embos
---------------
``pw_chrono_embos`` is a collection of ``pw_chrono`` backends that are
implemented using embOS v4 for 32bit targets.

.. warning::
  This module is under construction, not ready for use, and the documentation
  is incomplete.

SystemClock backend
-------------------
The embOS based ``system_clock`` backend implements the
``pw_chrono:system_clock`` facade by using ``OS_GetTime32()``. An
InterruptSpinLock is used to manage overflows in a thread and interrupt safe
manner to produce a signed 64 bit timestamp. Note that this does NOT use
``OS_GetTime_us64()`` which is not always available, this could be considered
for a future alternative backend for the SystemClock.

The ``SystemClock::now()`` must be used more than once per overflow of the
native embOS ``OS_GetTime32()`` overflow. Note that this duration may
vary if ``OS_SUPPORT_TICKLESS`` is used.

Build targets
-------------
The GN build for ``pw_chrono_embos`` has one target: ``system_clock``.
The ``system_clock`` target provides the
``pw_chrono_backend/system_clock_config.h`` and ``pw_chrono_embos/config.h``
headers and the backend for the ``pw_chrono:system_clock``.
