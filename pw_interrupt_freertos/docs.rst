.. _module-pw_interrupt_freertos:

---------------------
pw_interrupt_freertos
---------------------
.. pigweed-module::
   :name: pw_interrupt_freertos

This module implements a backend for ``pw_interrupt``. It requires a port of
FreeRTOS that defines the ``ullPortInterruptNesting``. Usually, ports for ARM
A-profile processors will have this defined. For Cortex-M processors, use the
``pw_interrupt_cortex_m`` backend.
