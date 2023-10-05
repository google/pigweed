.. _module-pw_chrono_rp2040:

================
pw_chrono_rp2040
================
This module provides backend implementations for pw_chrono that are suited for
use with an RP2040-based board (e.g. Raspberry Pi Pico).

-------------------
SystemClock backend
-------------------
The ``pw_chrono_rp2040:system_clock`` backend target implements the
``pw_chrono:system_clock`` facade by using the Pico SDK's time API. This is
backed by a 64-bit hardware timer that reports time-since-boot as microseconds.

See the :ref:`module-pw_chrono` documentation for ``pw_chrono`` for more
information on what functionality this provides.
