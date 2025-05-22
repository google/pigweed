.. _showcase-sense-tutorial-flash:

==================
7. Flash your Pico
==================
Enough with the simulations. Let's work with some real hardware!
You'll need a physical Raspberry Pi Pico for the rest of the tutorial.

.. caution::

   Make sure that you're using a Pico 1 or Pico 2, not a Pico 1W or Pico 2W.
   The Pico 1W and Pico 2W aren't supported.

.. _showcase-sense-tutorial-hardware:

--------------------
Set up your hardware
--------------------
Follow the instructions in one of the following tabs (**Full setup**,
**Pico and Enviro+**, or **Pico only**) to set up your hardware.
The full setup provides the most robust experience. Some later pages
in the tutorial require the full setup.

.. tab-set::

   .. tab-item:: Full setup

      .. _Pico: https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html
      .. _Enviro+ Pack: https://shop.pimoroni.com/products/pico-enviro-pack
      .. _Debug Probe: https://www.raspberrypi.com/products/debug-probe/
      .. _Omnibus: https://shop.pimoroni.com/products/pico-omnibus

      In the full setup, you combine a `Pico`_, `Enviro+ Pack`_, `Debug Probe`_, and
      `Omnibus`_. The Debug Probe provides more robust flashing and debugging.
      The Omnibus provides the Debug Probe access to the Pico's UART pins.
      These UART pins aren't accessible when a Pico is connected directly to an
      Enviro+.

      By the end of the instructions your full setup will look similar to this:

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/full_setup_zoom_v1.jpg

      **Update the Debug Probe firmware**

      .. _Update the Debug Probe firmware: https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html#updating-the-firmware-on-the-debug-probe

      #. Connect the Debug Probe to your development host over USB.

      #. `Update the Debug Probe firmware`_. You just need to download the latest
         release and drag-and-drop the UF2 file onto your Debug Probe. You
         want the ``debugprobe.uf2`` file from the releases page. It only
         takes a minute or two.

      **Omnibus setup**

      #. Connect the Enviro+ to **Deck 1** on the Omnibus.

      #. Connect the Pico to the middle landing area on the Omnibus.

         .. important::

            Make sure that the Pico's USB port lines up with the **USB**
            label on the Omnibus.

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/omnibus_v1.jpg

      **Serial Wire Debug port setup**

      .. _JST-SH: https://cdn-shop.adafruit.com/970x728/5765-01.jpg

      #. Find the Serial Wire Debug (SWD) port on your Pico. On the Pico 1 and
         Pico 2 the SWD port is at the edge of the board. The front of the Pico
         (the side with the raspberry logo) has a **DEBUG** label close to the
         port.

         .. caution::

            If your SWD port is in the middle of your board, it means you're
            using a Pico 1W or Pico 2W. Those boards aren't supported.

      #. Determine if your Serial Wire Debug (SWD) port has male or female connectors.

         If it's male, follow the instructions in the **Male** tab in the next step.
         If female, use the **Female** tab instructions.

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/swd_v1.jpg

      #. Connect the SWD port on your Debug Probe to the SWD port
         on your Pico.

         .. tab-set::

            .. tab-item:: Male
               :sync: male

               #. Connect the inner cable of the Debug Probe (SC) to the **SWCLK**
                  pin on the Pico's debug port. The back of the Pico
                  (the side that does not have the raspberry logo) has a **SWCLK**
                  label showing the location of that pin.

                  .. tip::

                     The image caption below provides an example of what we mean
                     by "inner cable" (as well as "middle" and "outer"). The colors on these
                     types of cables aren't standardized, so we had to think up a
                     different way to describe them.

               #. Connect the middle cable (ground) to the **GND** pin on the
                  Pico's debug port.

               #. Connect the outer cable (SD) to the **SWDIO** pin on the Pico's debug port.

               .. figure:: https://storage.googleapis.com/pigweed-media/sense/debug_male_v1.jpg

               .. figure:: https://storage.googleapis.com/pigweed-media/sense/debug_male_zoom_v2.jpg

                  In these images the "inner" cable is the white cable, the red cable is the "middle"
                  cable, and the black cable is the "outer" cable.

            .. tab-item:: Female
               :sync: female

               #. Connect the **DEBUG** port on the Pico with the
                  **DBUG** port on the Debug Probe using the `JST-SH`_
                  to JST-SH cable. JST-SH connectors only fit in one direction.

               .. figure:: https://storage.googleapis.com/pigweed-media/sense/debug_v3.jpg

      **UART setup**

      #. Connect the inner cable (RX, input to Debug Probe) to pin **0**
         on **Deck 2** of the Omnibus.

      #. Connect the outer cable (TX, output from Debug Probe) to pin **1**
         on **Deck 2** of the Omnibus.

      #. Connect the middle cable (ground) to any of the pins labeled with a long dash (**—**)
         on **Deck 2**.

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/uart_v1.jpg

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/uart_zoom_v1.jpg

         In the previous 2 images the "inner cable" is the yellow cable, the "middle"
         cable is the black cable, and the "outer" cable is the orange cable.

      .. note::

         The Serial Wire Debug port connections from the last section
         are omitted from the previous two images to help you focus on the new
         UART cable connections. Don't remove your SWD port connections.

      **USB setup**

      #. Hold down the **BOOTSEL** button on the front of your Pico (the side
         with the raspberry logo) to prepare the Pico for flashing.

      #. While still holding down **BOOTSEL**, connect your Pico to a USB port on your development host
         or to a separate power supply.

         .. tip::

            Connecting to a separate power supply will slightly simplify the flashing
            process later.

      #. Connect your the USB Micro-B port on your Debug Probe to a USB port on your
         development host. If you updated the Debug Probe firmware earlier, your Probe
         may already be connected to your host.

      You're done! Your setup should look similar to this:

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/full_setup_v1.jpg

   .. tab-item:: Pico and Enviro+

      #. Connect the Pico to the Enviro+ Pack.

      #. Hold down the **BOOTSEL** button on the front of your Pico (the side
         with the raspberry logo) to prepare the Pico for flashing.

      #. While still holding down **BOOTSEL**, connect your Pico to a USB port on your development host.

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/basic_enviro_front_v1.jpg

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/basic_enviro_back_v1.jpg

   .. tab-item:: Pico only

      #. Hold down the **BOOTSEL** button on the front of your Pico (the side
         with the raspberry logo) to prepare the Pico for flashing.

      #. While still holding down **BOOTSEL**, connect your Pico to a USB port on your development host.

      .. figure:: https://storage.googleapis.com/pigweed-media/sense/basic_v1.jpg

.. _showcase-sense-tutorial-udev:

-----------------
Set up udev rules
-----------------
#. Configure your host to properly detect Raspberry Pi hardware.

   .. tab-set::

      .. tab-item:: Linux

         #. Add the following rules to ``/etc/udev/rules.d/49-pico.rules`` or
            ``/usr/lib/udev/rules.d/49-pico.rules``. Create the file if it doesn't
            exist. You will probably need superuser privileges (``sudo``) to create
            or edit this file.

            .. literalinclude:: /targets/rp2040/49-pico.rules
               :language: linuxconfig
               :start-at: # Raspberry

         #. Reload the rules:

            .. code-block:: console

               sudo udevadm control --reload-rules && sudo udevadm trigger

         #. If your Pico is already connected to your host, unplug it and plug
            it back in again. Hold down the **BOOTSEL** button on the front of
            the Pico while plugging it back in to ensure that the Pico is ready
            for flashing.

      .. tab-item:: macOS

         No extra setup needed.

.. _showcase-sense-tutorial-flash-blinky:

----------------------------------------
Flash an application binary to your Pico
----------------------------------------
#. Flash the ``blinky`` bringup program to your Pico.

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         #. In **Bazel Targets** expand **//apps/blinky** and find your board's
            target:

            * If your board is a Pico 1 then your board's target is **flash_rp2040**.

            * If your board is a Pico 2 then your board's target is **flash_rp2350**.

              RP2040 is the name of the MCU that powers first-generation
              Picos. The RP2350 powers second-generation Picos.

         #. Right-click your board's target then select **Run target**.

         #. If you've connected both the Debug Probe and Pico to your host over USB,
            you'll see the following prompt. (If your Pico is connected to a separate
            power supply, you won't see this prompt and can ignore this part.) Select
            **Raspberry Pi - Debug Probe (CMSIS-DAP)**.

            .. code-block:: console

               Multiple devices detected. Please select one:
                 1 - bus 3, port 1 (Raspberry Pi - Debug Probe (CMSIS-DAP))
                 2 - bus 3, port 6 (Raspberry Pi - Pico)

            In this example you would want to select ``1``.

         A successful flash looks similar to this:

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/flash.png

      .. tab-item:: CLI
         :sync: cli

         #. Flash your Pico.

            .. tab-set::

               .. tab-item:: Pico 1 (RP2040)
                  :sync: rp2040

                  .. code-block:: console

                     bazelisk run //apps/blinky:flash_rp2040

               .. tab-item:: Pico 2 (RP2350)
                  :sync: rp2350

                  .. code-block:: console

                     bazelisk run //apps/blinky:flash_rp2350

         #. If you've connected both the Debug Probe and Pico to your host over USB,
            you'll see the following prompt. (If your Pico is connected to a separate
            power supply, you won't see this prompt and can ignore this part.) Select
            **Raspberry Pi - Debug Probe (CMSIS-DAP)**.

            .. code-block:: console

               Multiple devices detected. Please select one:
                 1 - bus 3, port 1 (Raspberry Pi - Debug Probe (CMSIS-DAP))
                 2 - bus 3, port 6 (Raspberry Pi - Pico)

         If the command completes and you see output like the following, then
         the flashing was successful. Sense does **not** log an explicit
         ``flashing successful`` message.

         .. code-block:: text

            20241220 19:46:38 INF Flashing bus 3 port 6

The LED on your Pico should start blinking on and off at a 1-second interval.

.. admonition:: Troubleshooting

   If you see ``A connected device has an inaccessible serial number: The
   device has no langid (permission issue, no string descriptors supported or
   device error)`` it probably means you need to update your udev rules. See
   :ref:`showcase-sense-tutorial-udev`.

   If you see ``Error: Connecting to the chip was unsuccessful`` or
   ``ERROR: This file cannot be loaded into the partition table on the device``,
   make sure that you're using the correct flashing target. These errors suggest
   that you tried to use the Pico 1 target on a Pico 2 board, or vice versa.

.. _showcase-sense-tutorial-flash-summary:

-------
Summary
-------
.. _target: https://bazel.build/concepts/build-ref#targets

In a Bazel-based project like Sense there is no separate flashing
process. Flashing is a Bazel target, just like building and testing
the source code are Bazel targets. Your team can manage all core
development workflows through Bazel.

Next, head over to :ref:`showcase-sense-tutorial-devicetests` to
try out on-device unit tests.
