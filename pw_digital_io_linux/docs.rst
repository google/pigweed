.. _module-pw_digital_io_linux:

.. cpp:namespace-push:: pw::digital_io

===================
pw_digital_io_linux
===================
.. pigweed-module::
   :name: pw_digital_io_linux

``pw_digital_io_linux`` implements the :ref:`module-pw_digital_io` interface
using the `Linux userspace gpio-cdev interface
<https://www.kernel.org/doc/Documentation/ABI/testing/gpio-cdev>`_.

.. note::

   Currently only the v1 userspace ABI is supported (https://pwbug.dev/328268007).


-------------
API reference
-------------
The following classes make up the public API:

``LinuxDigitalIoChip``
======================
Represents an open handle to a GPIO *chip* (e.g.  ``/dev/gpiochip0``).

This can be created using an existing file descriptor
(``LinuxDigitalIoChip(fd)``) or by opening a given path
(``LinuxDigitalIoChip::Open(path)``).

``LinuxDigitalIn``
==================
Represents a single input line and implements :cpp:class:`DigitalIn`.

This is acquired by calling ``chip.GetInputLine()`` with an appropriate
``LinuxInputConfig``.

``LinuxDigitalOut``
===================
Represents a single input line and implements :cpp:class:`DigitalOut`.

This is acquired by calling ``chip.GetOutputLine()`` with an appropriate
``LinuxOutputConfig``.

.. warning::

   The ``Disable()`` implementation only closes the GPIO handle file
   descriptor, relying on the underlying GPIO driver / pinctrl to restore
   the state of the line. This may or may not be implemented depending on
   the GPIO driver.


------
Guides
------
Example code to use GPIO pins:

Configure an input pin and get its state
========================================

.. code-block:: cpp

   #include "pw_digital_io_linux/digital_io.h"
   #include "pw_status/try.h"

   using pw::digital_io::LinuxDigitalIoChip;

   pw::Status InputExample() {
     // Open handle to chip.
     PW_TRY_ASSIGN(auto chip, LinuxDigitalIoChip::Open("/dev/gpiochip0"));

     // Configure input line.
     LinuxInputConfig config(
         /* index= */ 5,
         /* polarity= */ Polarity::kActiveHigh);
     PW_TRY_ASSIGN(auto input, chip.GetInputLine(config));
     PW_TRY(input.Enable());

     // Get the input pin state.
     PW_TRY_ASSIGN(auto pin_state, input.GetState());

     return pw::OkStatus();
   }

Configure an output pin and set its state
=========================================

.. code-block:: cpp

   #include "pw_digital_io/polarity.h"
   #include "pw_digital_io_linux/digital_io.h"
   #include "pw_status/try.h"

   using pw::digital_io::LinuxDigitalIoChip;
   using pw::digital_io::LinuxOutputConfig;
   using pw::digital_io::Polarity;
   using pw::digital_io::State;

   pw::Status OutputExample() {
     // Open handle to chip.
     PW_TRY_ASSIGN(auto chip, LinuxDigitalIoChip::Open("/dev/gpiochip0"));

     // Configure output line.
     // Set the polarity to active-low and default state to active.
     LinuxOutputConfig config(
         /* index= */ 4,
         /* polarity= */ Polarity::kActiveLow,
         /* default_state= */ State::kActive);
     PW_TRY_ASSIGN(auto output, chip->GetOutputLine(config));

     // Enable the output pin. This pulls the pin to ground since the
     // polarity is kActiveLow and the default_state is kActive.
     PW_TRY(output.Enable());

     // Set the output pin to inactive.
     // This pulls pin to Vdd since the polarity is kActiveLow.
     PW_TRY(out.SetState(State::kInactive));

     return pw::OkStatus();
   }


.. cpp:namespace-pop::
