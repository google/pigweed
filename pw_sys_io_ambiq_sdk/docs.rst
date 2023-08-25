.. _module-pw_sys_io_ambiq_sdk:

===================
pw_sys_io_ambiq_sdk
===================
``pw_sys_io_ambiq_sdk`` implements the ``pw_sys_io`` facade over UART using the
Ambiq Suite SDK HAL.

The UART baud rate is fixed at 115200 (8N1).

Setup
=====
This module requires relatively minimal setup:

1. Write code against the ``pw_sys_io`` facade.
2. Specify the ``dir_pw_sys_io_backend`` GN global variable to point to this
   backend.
3. Call ``pw_sys_io_Init()`` during init so the UART is properly initialized and
   configured.

The UART peripheral and the GPIO pins are defined in the ``am_bsp.h`` file. Make sure
that the build argument ``pw_third_party_ambiq_PRODUCT`` is set correctly so that
the correct bsp header file is included.
