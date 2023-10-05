.. _module-pw_digital_io_rp2040:

--------------------
pw_digital_io_rp2040
--------------------
``pw_digital_io_rp2040`` implements the :ref:`module-pw_digital_io` interface using
the `Raspberry Pi Pico SDK <https://github.com/raspberrypi/pico-sdk/>`_.

Setup
=====
Use of this module requires setting up the Pico SDK for use with Pigweed. Follow
the steps in :ref:`target-raspberry-pi-pico` to get setup.

Examples
========
Use ``pw::digital_io::Rp2040DigitalIn`` and
``pw::digital_io::Rp2040DigitalInOut`` classes to control GPIO pins.

Example code to use GPIO pins:

.. code-block:: cpp

   #include "pw_digital_io_rp2040/digital_io.h"

   using pw::digital_io::Rp2040DigitalIn;
   using pw::digital_io::Rp2040DigitalInOut;

   Rp2040DigitalInOut out(/*gpio_pin=*/ 15);
   out.Enable();
   out.SetState(pw::digital_io::State::kInactive);

   Rp2040DigitalIn in(/*gpio_pin=*/ 16);
   in.Enable();
   auto state = in.GetState();
