.. _target-raspberry-pi-pico:

================================
Raspberry Pi Pico with pw_system
================================
.. warning::

   This target is in an early state and is under active development. Usability
   is not very polished, and many features/configuration options that work in
   upstream Pi Pico CMake build have not yet been ported to the GN build.

This target configuration uses :ref:`pw_system<module-pw_system>` on top of
FreeRTOS and the `Raspberry Pi Pico SDK
<https://github.com/raspberrypi/pico-sdk>`_ HAL rather than a from-the-ground-up
baremetal approach.

-----
Setup
-----
To use this target, Pigweed must be set up to use FreeRTOS and the Pico SDK
HAL. The supported repositories can be downloaded via ``pw package``, and then
the build must be manually configured to point to the locations the repositories
were downloaded to.

.. code-block:: sh

   pw package install freertos
   pw package install pico_sdk

   gn gen out --export-compile-commands --args="
     dir_pw_third_party_freertos=\"//environment/packages/freertos\"
     PICO_SRC_DIR=\"//environment/packages/pico_sdk\"
   "

.. tip::

   Instead of the ``gn gen out`` with args set on the command line above you can
   run:

   .. code-block:: sh

      gn args out

   Then add the following lines to that text file:

   .. code-block::

      dir_pw_third_party_freertos = getenv("PW_PACKAGE_ROOT") + "/freertos"
      PICO_SRC_DIR = getenv("PW_PACKAGE_ROOT") + "/pico_sdk"

Linux
=====
On linux, you may need to update your udev rules at
``/etc/udev/rules.d/49-pico.rules`` to include the following:

.. code-block:: none

   SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0004", MODE:="0666"
   KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0004", MODE:="0666"
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE:="0666"
   KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE:="0666"
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", MODE:="0666"
   KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", MODE:="0666"

--------
Building
--------
The Pi Pico is configured to output logs and test results over USB serial at a
baud rate of 115200.

Once the pico SDK is configured, the Pi Pico will build as part of the default
GN build:

.. code-block:: sh

   ninja -C out

--------
Flashing
--------
Flashing the Pi Pico is two easy steps:

#. While holding the button on the Pi Pico, connect the Pico to your computer
   via the micro USB port.
#. Copy the desired UF2 firmware image to the RPI-RP2 volume that enumerated
   when you connected the Pico.

------------------
Running unit tests
------------------
Unlike most other targets in Pigweed, the RP2040 uses RPC-based unit testing.
This makes it easier to fully automate on-device tests in a scalable and
maintainable way.

Step 1: Start test server
=========================
To allow Ninja to properly serialize tests to run on device, Ninja will send
test requests to a server running in the background. The first step is to launch
this server. By default, the script will attempt to automatically detect an
attached Pi Pico running an application with USB serial enabled, then using
it for testing. To override this behavior, provide a custom server configuration
file with ``--server-config``.

.. code-block:: sh

   $ python -m rp2040_utils.unit_test_server

.. tip::

   If the server can't find any attached devices, ensure your Pi Pico is
   already running an application that utilizes USB serial.

.. Warning::

   If you connect or disconnect any boards, you'll need to restart the test
   server for hardware changes to take effect.

Step 2: Configure GN
====================
By default, this hardware target has incremental testing disabled. Enabling the
``pw_targets_ENABLE_RP2040_TEST_RUNNER`` build arg tells GN to send requests to
a running ``rp2040_utils.unit_test_server``.

.. code-block:: sh

   $ gn args out
   # Modify and save the args file to use pw_target_runner.
   pw_targets_ENABLE_RP2040_TEST_RUNNER = true

Step 3: Build changes
=====================
Now, whenever you run ``ninja -C out pi_pico``, all tests affected by changes
since the last build will be rebuilt and then run on the attached device.
Alternatively, you may use ``pw watch`` to set up Pigweed to trigger
builds/tests whenever changes to source files are detected.

-----------------------------------------
Building and running the demo application
-----------------------------------------
This target has an associated demo application that can be built and then
flashed to a device with the following commands:

Build
=====
.. code-block:: sh

   ninja -C out pw_system_demo

Flash
=====
- Using a uf2 file:

  Copy to ``out/rp2040.size_optimized/obj/pw_system/system_example.uf2``
  your Pico when it is in USB bootloader mode. Hold down the BOOTSEL button when
  plugging in the pico and it will appear as a mass storage device.

- Using a Pico Probe and openocd:

  This requires installing the Raspberry Pi foundation's OpenOCD fork for the
  Pico probe. More details including how to connect the two Pico boards is
  available in ``Appendix A: Using Picoprobe`` of the `Getting started with
  Raspberry Pi Pico
  <https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf>`_
  guide.

  **Install RaspberryPi's OpenOCD Fork:**

  .. code-block:: sh

     git clone https://github.com/raspberrypi/openocd.git \
       --branch picoprobe \
       --depth=1 \
       --no-single-branch \
       openocd-picoprobe

     cd openocd-picoprobe

     ./bootstrap
     ./configure --enable-picoprobe --prefix=$HOME/apps/openocd --disable-werror
     make -j2
     make install

  **Setup udev rules (Linux only):**

  .. code-block:: sh

     cat <<EOF > 49-picoprobe.rules
     SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000[43a]", MODE:="0666"
     KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000[43a]", MODE:="0666"
     EOF
     sudo cp 49-picoprobe.rules /usr/lib/udev/rules.d/49-picoprobe.rules
     sudo udevadm control --reload-rules

  **Flash the Pico:**

  .. code-block:: sh

     ~/apps/openocd/bin/openocd -f ~/apps/openocd/share/openocd/scripts/interface/picoprobe.cfg -f ~/apps/openocd/share/openocd/scripts/target/rp2040.cfg -c 'program out/rp2040.size_optimized/obj/pw_system/bin/system_example.elf verify reset exit'

Connect with pw_console
=======================
Once the board has been flashed, you can connect to it and send RPC commands
via the Pigweed console:

.. code-block:: sh

   pw-system-console -d /dev/{ttyX} -b 115200 \
     --proto-globs pw_rpc/echo.proto \
     --token-databases \
       out/rp2040.size_optimized/obj/pw_system/bin/system_example.elf

Replace ``{ttyX}`` with the appropriate device on your machine. On Linux this
may look like ``ttyACM0``, and on a Mac it may look like ``cu.usbmodem***``.

When the console opens, try sending an Echo RPC request. You should get back
the same message you sent to the device.

.. code-block:: pycon

   >>> device.rpcs.pw.rpc.EchoService.Echo(msg="Hello, Pigweed!")
   (Status.OK, pw.rpc.EchoMessage(msg='Hello, Pigweed!'))

You can also try out our thread snapshot RPC service, which should return a
stack usage overview of all running threads on the device in Host Logs.

.. code-block:: pycon

   >>> device.snapshot_peak_stack_usage()

Example output:

.. code-block::

   20220826 09:47:22  INF  PendingRpc(channel=1, method=pw.thread.ThreadSnapshotService.GetPeakStackUsage) completed: Status.OK
   20220826 09:47:22  INF  Thread State
   20220826 09:47:22  INF    5 threads running.
   20220826 09:47:22  INF
   20220826 09:47:22  INF  Thread (UNKNOWN): IDLE
   20220826 09:47:22  INF  Est CPU usage: unknown
   20220826 09:47:22  INF  Stack info
   20220826 09:47:22  INF    Current usage:   0x20002da0 - 0x???????? (size unknown)
   20220826 09:47:22  INF    Est peak usage:  390 bytes, 76.77%
   20220826 09:47:22  INF    Stack limits:    0x20002da0 - 0x20002ba4 (508 bytes)
   20220826 09:47:22  INF
   20220826 09:47:22  INF  ...

You are now up and running!

.. seealso::

   The :ref:`module-pw_console`
   :bdg-ref-primary-line:`module-pw_console-user_guide` for more info on using
   the the pw_console UI.
