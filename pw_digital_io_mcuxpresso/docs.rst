.. _module-pw_digital_io_mcuxpresso:

------------------------
pw_digital_io_mcuxpresso
------------------------
``pw_digital_io_mcuxpresso`` implements the :ref:`module-pw_digital_io` interface using
the NXP MCUXpresso SDK.

Setup
=====
Use of this module requires setting up the MCUXpresso SDK for use with Pigweed. Follow
the steps in :ref:`module-pw_build_mcuxpresso` to create a ``pw_source_set`` for an
MCUXpresso SDK. Include the GPIO and PINT driver components in this SDK definition.

This example shows what your SDK setup would look like if using an RT595 board.

.. code-block:: text

   import("$dir_pw_third_party/mcuxpresso/mcuxpresso.gni")

   pw_mcuxpresso_sdk("sample_project_sdk") {
     manifest = "$dir_pw_third_party/mcuxpresso/evkmimxrt595/EVK-MIMXRT595_manifest_v3_8.xml"
     include = [
       "component.serial_manager_uart.MIMXRT595S",
       "platform.drivers.lpc_gpio.MIMXRT595S",
       "platform.drivers.pint.MIMXRT595S",
       "project_template.evkmimxrt595.MIMXRT595S",
       "utility.debug_console.MIMXRT595S",
     ]
   }

Next, specify the ``pw_third_party_mcuxpresso_SDK`` GN global variable to specify
the name of this source set. Edit your GN args with ``gn args out``.

.. code-block:: text

   pw_third_party_mcuxpresso_SDK = "//targets/mimxrt595_evk:sample_project_sdk"

Then, depend on this module in your BUILD.gn to use.

.. code-block:: text

   deps = [ dir_pw_digital_io_mcuxpresso ]

Examples
========
Use ``pw::digital_io::McuxpressoDigitalIn`` and ``pw::digital_io::McuxpressoDigitalOut``
classes to control GPIO pins.

Example code to use GPIO pins from an NXP SDK board definition:

.. code-block:: text

   McuxpressoDigitalOut out(BOARD_INITPINS_D8_GPIO,
                           BOARD_INITPINS_D8_PORT,
                           BOARD_INITPINS_D8_PIN,
                           pw::digital_io::State::kActive);
   out.SetState(pw::digital_io::State::kInactive);

   McuxpressoDigitalIn in(
      BOARD_INITPINS_D9_GPIO, BOARD_INITPINS_D9_PORT, BOARD_INITPINS_D9_PIN);
   auto state = in.GetState();

