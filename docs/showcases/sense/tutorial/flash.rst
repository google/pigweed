.. _showcase-sense-tutorial-flash:

==================
7. Flash your Pico
==================
Enough with the simulations. Let's work with some real hardware!
You'll need a physical Raspberry Pi Pico for the rest of the tutorial.
You can use either the Pico 1 or Pico 2; we support both.

If you don't have a Pico, you can just skim the remaining pages of
the tutorial without actually doing the workflows, or skip ahead to
:ref:`showcase-sense-tutorial-outro`.

.. _showcase-sense-tutorial-hardware:

--------------------
Set up your hardware
--------------------
You can use any of the following hardware setups.

.. _Pico W: https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html#raspberry-pi-pico-w-and-pico-wh

.. caution::

   **The Pico W is untested**. We are still in the process of verifying that
   all parts of the tutorial work with the `Pico W`_. You are welcome to try
   the tutorial with a Pico W, but please remember that some things may not
   work yet.

.. _showcase-sense-tutorial-basic:

Option: Basic setup
===================
You can connect your development host directly to a Pico via a
USB cable.

.. figure:: https://storage.googleapis.com/pigweed-media/airmaranth/basic_setup.jpg
   :alt: USB connected to Pico, no Enviro+ Pack involved

If you have an Enviro+ Pack, you connect the Enviro+ Pack to
the headers of your Pico. The Pico is still connected to your
development host over USB, same as before.

.. figure:: https://storage.googleapis.com/pigweed-media/airmaranth/basic_setup_enviro.jpg
   :alt: USB connected to Pico, with Pico connected to Enviro+

.. _showcase-sense-tutorial-full:

Option: Full setup
==================
.. _Pico Omnibus: https://shop.pimoroni.com/products/pico-omnibus

.. Don't link to Raspberry Pi Debug Probe here because some dogfooders
.. went to the product's homepage and thought they had to set up
.. OpenOCD and other painful stuff like that.

For the most robust long-term setup, use a Raspberry Pi Debug Probe
and `Pico Omnibus`_ in combination with your Pico and Enviro+ Pack:

.. _Update the firmware on the Debug Probe: https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html#updating-the-firmware-on-the-debug-probe

#. Connect the Debug Probe to your host over USB.

#. `Update the firmware on the Debug Probe`_. You just need to
   download the latest release and drag-and-drop the UF2 file
   onto your Debug Probe. You want the ``debugprobe.uf2`` file
   from the releases page. It only takes a minute or two.

#. Connect the Enviro+ to **DECK 1** on the Omnibus.

#. Connect the Pico to the middle landing area on the Omnibus.
   Make sure that the USB port on your Pico is lined up with the
   USB label on the Omnibus.

#. Connect the Pico and Debug Probe together with the
   3-pin debug to 0.1-inch header female cable (the yellow, black,
   and orange cable). On deck 2 connect the yellow wire to pin 0
   (Pico TX, Debug Probe RX), the orange wire to pin 1 (Pico RX,
   Debug Probe TX), and the black wire to **-** (GND).
   See the next image.

#. Connect the Pico and Debug Probe together with the 3-pin
   debug to 3-pin debug cable (the grey and red cable). See the
   image below.

#. Connect the Pico's micro USB port to a power supply.

   You can supply power to the Pico by connecting it to a USB port
   on your host. Later on this will make your flashing process a little
   more complex. So it's simpler to provide power to the Pico separately,
   if you can.


.. figure:: https://storage.googleapis.com/pigweed-media/airmaranth/full_setup.jpg
   :alt: Full hardware setup with Debug Probe, Pico, Omnibus, and Enviro+ Pack

   The full hardware setup

.. _showcase-sense-tutorial-udev:

-----------------
Set up udev rules
-----------------
Configure your host to properly detect Raspberry Pi hardware.

.. tab-set::

   .. tab-item:: Linux

      #. Add the following rules to ``/etc/udev/rules.d/49-pico.rules`` or
         ``/usr/lib/udev/rules.d/49-pico.rules``. Create the file if it doesn't
         exist.

         .. literalinclude:: /targets/rp2040/49-pico.rules
            :language: linuxconfig
            :start-at: # Raspberry

      #. Reload the rules:

         .. code-block:: console

            sudo udevadm control --reload-rules
            sudo udevadm trigger

      #. If your Pico is already connected to your host, unplug it and plug
         it back in again.

   .. tab-item:: macOS

      No extra setup needed.

.. _showcase-sense-tutorial-flash-blinky:

----------------------------------------
Flash an application binary to your Pico
----------------------------------------
#. Flash the blinky binary to your Pico.

   .. tab-set::

      .. tab-item:: VS Code
         :sync: vsc

         In **Bazel Build Targets** expand **//apps/blinky**, then right-click
         **:flash (alias)**, then select **Run target**.

         If you see an interactive prompt to select a device, see
         the note below.

         A successful flash looks similar to this:

         .. figure:: https://storage.googleapis.com/pigweed-media/sense/20240802/flash.png

      .. tab-item:: CLI
         :sync: cli

         .. code-block:: console

            $ bazelisk run //apps/blinky:flash
            INFO: Analyzed target //apps/blinky:flash (0 packages loaded, 0 targets configured).
            INFO: Found 1 target...
            Target //apps/blinky:flash_rp2040 up-to-date:
              bazel-bin/apps/blinky/flash_rp2040.exe
            INFO: Elapsed time: 0.129s, Critical Path: 0.00s
            INFO: 1 process: 1 internal.
            INFO: Build completed successfully, 1 total action
            INFO: Running command line: bazel-bin/apps/blinky/flash_rp2040.exe apps/blinky/rp2040_blinky.elf
            20240806 18:16:58 INF Only one device detected.
            20240806 18:16:58 INF Flashing bus 3 port 6

         If you see an interactive prompt to select a device, see
         the note below.

.. admonition:: :ref:`Full setup <showcase-sense-tutorial-full>` flashing

   If the Pico and Debug Probe are both connected to your development
   host, you'll see an interactive prompt asking you to
   select a device. Choose ``Raspberry Pi - Debug Probe (CMSIS-DAP)``.
   When the Debug Probe receives the flashing command, it knows that
   the command is intended for the Pico it's connected to, not itself.

   .. code-block:: console

      INFO: Running command line: bazel-bin/apps/blinky/flash_rp2040.exe apps/blinky/rp2040_blinky.elf
      Multiple devices detected. Please select one:
        1 - bus 3, port 1 (Raspberry Pi - Pico)
        2 - bus 3, port 6 (Raspberry Pi - Debug Probe (CMSIS-DAP))

      Enter an item index or press up/down (Ctrl-C to cancel)
      > 2
      20240729 16:29:46 INF Flashing bus 3 port 6

You should see your Raspberry Pi Pico's LED start blinking on and off at a
1-second interval.

.. _Your First Binaries: https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html#your-first-binaries

.. admonition:: Troubleshooting

   If the firmware on your Pico is in good working order, you
   won't need to hold down **BOOTSEL** while connecting the
   USB. If the flashing doesn't work, try the **BOOTSEL** workflow
   that's described in `Your First Binaries`_.

.. _showcase-sense-tutorial-flash-summary:

-------
Summary
-------
.. _target: https://bazel.build/concepts/build-ref#targets

In a Bazel-based project like Sense there is no separate flashing
tool or command that you need to memorize; flashing is a Bazel
`target`_ just like everything else.

As mentioned in :ref:`showcase-sense-tutorial-build-summary`,
you actually don't need to build binaries before running flashing
targets like this. You can just skip straight to running the flash
target and Bazel will figure out what binaries to build before
attempting to flash.

Next, head over to :ref:`showcase-sense-tutorial-devicetests` to
try out on-device unit tests.
