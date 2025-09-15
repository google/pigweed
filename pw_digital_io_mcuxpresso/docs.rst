.. _module-pw_digital_io_mcuxpresso:

========================
pw_digital_io_mcuxpresso
========================
.. pigweed-module::
   :name: pw_digital_io_mcuxpresso

``pw_digital_io_mcuxpresso`` implements the :ref:`module-pw_digital_io` interface using
the NXP MCUXpresso SDK.

--------
Overview
--------

This module consists of these main classes:

- :doxylink:`McuxpressoDigitalIn <pw::digital_io::McuxpressoDigitalIn>`:
  Provides only input support.
- :doxylink:`McuxpressoDigitalOut <pw::digital_io::McuxpressoDigitalOut>`:
  Provides only output support.
- :doxylink:`McuxpressoDigitalInOutInterrupt
  <pw::digital_io::McuxpressoDigitalInOutInterrupt>`:
  Provides support for input, output, and GPIO interrupts.
- :doxylink:`McuxpressoPintController
  <pw::digital_io::McuxpressoPintController>`: Controller class for use with
  ``McuxpressoPintInterrupt``.
- :doxylink:`McuxpressoPintInterrupt
  <pw::digital_io::McuxpressoPintInterrupt>`: Provides only interrupt support
  for a PINT interrupt.

-----
Setup
-----
Use of this module requires setting up the MCUXpresso SDK for use with Pigweed. Follow
the steps in :ref:`module-pw_build_mcuxpresso` to create a ``pw_source_set`` for an
MCUXpresso SDK. Include the GPIO and PINT driver components in this SDK definition.

--------
Examples
--------

Digital input
=============
Use :doxylink:`McuxpressoDigitalIn <pw::digital_io::McuxpressoDigitalIn>` to
read the state of an input.

Example code to use GPIO pins from an NXP SDK board definition:

.. code-block:: cpp

   #include "board/pin_mux.h"
   #include "pw_digital_io_mcuxpresso/digital_io.h"

   McuxpressoDigitalIn input(BOARD_INITPINS_D9_GPIO,
                             BOARD_INITPINS_D9_PORT,
                             BOARD_INITPINS_D9_PIN);

   Status Init() { return input.Enable(); }

   Status InputExample() {
     PW_TRY_ASSIGN(const DigitalIo::State state, input.GetState());
     // ...
   }

Digital output
==============
Use :doxylink:`McuxpressoDigitalOut <pw::digital_io::McuxpressoDigitalOut>` to
set the state of an output.

Example code to use GPIO pins from an NXP SDK board definition:

.. code-block:: cpp

   #include "board/pin_mux.h"
   #include "pw_digital_io_mcuxpresso/digital_io.h"
   #include "pw_status/status.h"

   McuxpressoDigitalOut output(BOARD_INITPINS_D8_GPIO,
                               BOARD_INITPINS_D8_PORT,
                               BOARD_INITPINS_D8_PIN,
                               pw::digital_io::State::kActive);

   Status Init() { return output.Enable(); }

   Status OutputExample() {
     return output.SetState(pw::digital_io::State::kInactive);
   }

GPIO interrupt
==============
Use :doxylink:`McuxpressoDigitalInOutInterrupt
<pw::digital_io::McuxpressoDigitalInOutInterrupt>` to handle interrupts via
the GPIO module.

Example code to use GPIO pins from an NXP SDK board definition:

.. code-block:: cpp

   #include "board/pin_mux.h"
   #include "pw_digital_io_mcuxpresso/digital_io.h"
   #include "pw_status/status.h"

   McuxpressoDigitalInOutInterrupt irq_pin(BOARD_INITPINS_D9_GPIO,
                                           BOARD_INITPINS_D9_PORT,
                                           BOARD_INITPINS_D9_PIN,
                                           /* output= */ false);

   Status Init() {
     PW_TRY(irq_pin.Enable());
     PW_TRY(irq_pin.SetInterruptHandler(
         pw::digital_io::InterruptTrigger::kDeactivatingEdge,
         [](State /* state */) { irq_count++; }));
     PW_TRY(irq_pin.EnableInterruptHandler());
     return OkStatus();
   }

PINT interrupt
==============
``pw::digital_io::McuxpressoPintInterrupt`` can also be used to handle
interrupts, via the PINT module, which supports other features:

* Dedicated (non-shared) IRQs for each interrupt
* Double edge detection (``InterruptTrigger::kBothEdges``)
* Waking from deep sleep with edge detection
* Pattern matching support (currently unsupported here)
* Triggering interrupts on pins configured for a non-GPIO function

It must be used with an instance of :doxylink:`McuxpressoPintController
<pw::digital_io::McuxpressoPintController>`.

.. code-block:: cpp

   #include "pw_digital_io_mcuxpresso/pint.h"
   #include "pw_sync/interrupt_spin_lock.h"

   McuxpressoPintController raw_pint_controller(PINT);

   pw::sync::VirtualInterruptSpinLock controller_lock;

   pw::sync::Borrowable<McuxpressoPintController> pint_controller(
       raw_pint_controller, controller_lock);

   McuxpressoPintInterrupt irq_line0(pint_controller, kPINT_PinInt0);

   Status Init() {
     // Attach pin PIO0_4 to PINT interrupt 0.
     INPUTMUX_AttachSignal(
         INPUTMUX, kPINT_PinInt0, kINPUTMUX_GpioPort0Pin4ToPintsel);

     PW_TRY(irq_line0.Enable());
     PW_TRY(irq_line0.SetInterruptHandler(
         pw::digital_io::InterruptTrigger::kBothEdges,
         [](State /* state */) { irq_count++; }));
     PW_TRY(irq_line0.EnableInterruptHandler());
     return OkStatus();
   }

-------------
API reference
-------------
Moved: :doxylink:`pw_digital_io_mcuxpresso`
