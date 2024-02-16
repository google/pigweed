.. _module-pw_chrono_rp2040:

================
pw_chrono_rp2040
================
This module provides a pw_chrono backend suitable for use with an RP2040-based
board (e.g. Raspberry Pi Pico). This backend works with baremetal operation of
a Pi Pico, and may be suitable for RTOS contexts with some additional
configuration of the RTOS timer settings.

-------------------
SystemClock backend
-------------------
The ``pw_chrono_rp2040:system_clock`` backend target implements the
``pw_chrono:system_clock`` facade by using the Pico SDK's time API. This is
backed by a 64-bit hardware timer that reports time-since-boot as microseconds.
As the peripheral API in the Pico SDK expects microseconds, this is the most
appropriate clock to use when interacting with peripherals.

See the :ref:`module-pw_chrono` documentation for ``pw_chrono`` for more
information on what functionality this provides.
