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

   #include "pw_digital_io/polarity.h"
   #include "pw_digital_io_rp2040/digital_io.h"

   using pw::digital_io::Polarity;
   using pw::digital_io::Rp2040Config;
   using pw::digital_io::Rp2040DigitalIn;
   using pw::digital_io::Rp2040DigitalInOut;
   using pw::digital_io::State;

   constexpr Rp2040Config output_pin_config{
       .pin = 15,
       .polarity = Polarity::kActiveLow,
   };
   constexpr Rp2040Config input_pin_config{
       .pin = 16,
       .polarity = Polarity::kActiveHigh,
   };

   // Config output pin:
   Rp2040DigitalInOut out(output_pin_config);
   out.Enable();

   // Set the output pin active.
   // This pulls pin to ground since the polarity is kActiveLow.
   out.SetState(State::kActive);

   // Config input pin:
   Rp2040DigitalIn in(input_pin_config);
   in.Enable();

   // Get the pin state. Since the polarity is kActiveHigh this will return
   // State::kActive if the pin is high or and State::kInactive if the pin is
   // low (grounded).
   State pin_state = in.GetState();

