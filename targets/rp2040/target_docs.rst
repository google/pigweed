.. _target-raspberry-pi-pico:

=================
Raspberry Pi Pico
=================
.. warning::

   This target is in an early state and is under active development. Usability
   is not very polished, and many features/configuration options that work in
   upstream Pi Pico CMake build have not yet been ported to the GN build.

This target configuration uses :ref:`pw_system<module-pw_system>` on top of
FreeRTOS and the `Raspberry Pi Pico SDK
<https://github.com/raspberrypi/pico-sdk>`_ HAL rather than a baremetal
approach.

----------------
First-time setup
----------------
To use this target, Pigweed must be set up to use FreeRTOS and the Pico SDK
HAL. The supported repositories can be downloaded via ``pw package``, and then
the build must be manually configured to point to the locations the repositories
were downloaded to.

.. warning::

   The GN build does not distribute the libusb headers which are required by
   picotool.  If the picotool installation fails due to missing libusb headers,
   it can be fixed by installing them manually.

   .. tab-set::

      .. tab-item:: Linux
         :sync: linux

         .. code-block:: sh

            sudo apt-get install libusb-1.0-0-dev

         .. admonition:: Note
            :class: tip

            These instructions assume a Debian/Ubuntu Linux distribution.

      .. tab-item:: macOS
         :sync: macos

         .. code-block:: sh

            brew install libusb
            brew install pkg-config

         .. admonition:: Note
            :class: tip

            These instructions assume a brew is installed and used for package
            management.

.. code-block:: console

   $ pw package install freertos
   $ pw package install pico_sdk
   $ pw package install picotool

   $ gn gen out --export-compile-commands --args="
       dir_pw_third_party_freertos=\"//environment/packages/freertos\"
       PICO_SRC_DIR=\"//environment/packages/pico_sdk\"
     "

.. tip::

   Instead of the ``gn gen out`` with args set on the command line above you can
   run:

   .. code-block:: console

      $ gn args out

   Then add the following lines to that text file:

   .. code-block::

      dir_pw_third_party_freertos = getenv("PW_PACKAGE_ROOT") + "/freertos"
      PICO_SRC_DIR = getenv("PW_PACKAGE_ROOT") + "/pico_sdk"

.. _target-raspberry-pi-pico-first_time_setup-setting_up_linux_udev_rules:

Setting up udev rules
=====================
On linux, you may need to update your udev rules at
``/etc/udev/rules.d/49-pico.rules`` to include the following:

.. code-block:: none

   # RaspberryPi Debug probe: https://github.com/raspberrypi/debugprobe
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000c", MODE:="0666"
   KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000c", MODE:="0666"
   # RaspberryPi Legacy Picoprobe (early Debug probe version)
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0004", MODE:="0666"
   KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0004", MODE:="0666"
   # RP2040 Bootloader mode
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE:="0666"
   KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE:="0666"
   # RP2040 USB Serial
   SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", MODE:="0666"
   KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", MODE:="0666"

--------
Building
--------
Once the Pico SDK is configured, the Pi Pico will build as part of the default
GN build:

.. code-block:: console

   $ ninja -C out

The pw_system example is available as a separate build target:

.. code-block:: console

   $ ninja -C out pw_system_demo

--------
Flashing
--------
Using the mass-storage booloader
================================
Hold down the BOOTSEL button when plugging in the pico and it will appear as a
mass storage device. Copy the UF2 firmware image (for example,
``out/rp2040.size_optimized/obj/pw_system/system_example.uf2``) to
your Pico when it is in USB bootloader mode.

.. tip::

   This is the simplest solution if you are fine with physically interacting
   with your Pico whenever you want to flash a new firmware image.

.. _target-raspberry-pi-pico-flashing-using_openocd:

Using OpenOCD
=============
To flash using OpenOCD, you'll either need a
`Pico debug probe <https://www.raspberrypi.com/products/debug-probe/>`_ or a
second Raspberry Pi Pico to use as a debug probe. Also, on Linux you'll need to
follow the instructions for
:ref:`target-raspberry-pi-pico-first_time_setup-setting_up_linux_udev_rules`\.

First-time setup
----------------
First, flash your first Pi Pico with ``debugprobe_on_pico.uf2`` from `the
latest release of debugprobe <https://github.com/raspberrypi/debugprobe/releases/latest>`_.

Next, connect the two Pico boards as follows:

- Pico probe GND -> target Pico GND
- Pico probe GP2 -> target Pico SWCLK
- Pico probe GP3 -> target Pico SWDIO

If you do not jump VSYS -> VSYS, you'll need to connect both Pi Pico boards
to USB ports so that they have power.

For more detailed instructions on how how to connect two Pico boards, see
``Appendix A: Using Picoprobe`` of the `Getting started with Raspberry Pi Pico
<https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf>`_
guide.

Flashing a new firmware
-----------------------
Once your Pico is all wired up, you'll be able to flash it using OpenOCD:

.. code-block:: console

   $ openocd -f interface/cmsis-dap.cfg \
         -f target/rp2040.cfg -c "adapter speed 5000" \
         -c "program out/rp2040.size_optimized/obj/pw_system/bin/system_example.elf verify reset exit"

Typical output:

.. code-block:: none

   xPack Open On-Chip Debugger 0.12.0+dev-01312-g18281b0c4-dirty (2023-09-05-01:33)
   Licensed under GNU GPL v2
   For bug reports, read
      http://openocd.org/doc/doxygen/bugs.html
   Info : Hardware thread awareness created
   Info : Hardware thread awareness created
   adapter speed: 5000 kHz
   Info : Using CMSIS-DAPv2 interface with VID:PID=0x2e8a:0x000c, serial=415032383337300B
   Info : CMSIS-DAP: SWD supported
   Info : CMSIS-DAP: Atomic commands supported
   Info : CMSIS-DAP: Test domain timer supported
   Info : CMSIS-DAP: FW Version = 2.0.0
   Info : CMSIS-DAP: Interface Initialised (SWD)
   Info : SWCLK/TCK = 0 SWDIO/TMS = 0 TDI = 0 TDO = 0 nTRST = 0 nRESET = 0
   Info : CMSIS-DAP: Interface ready
   Info : clock speed 5000 kHz
   Info : SWD DPIDR 0x0bc12477, DLPIDR 0x00000001
   Info : SWD DPIDR 0x0bc12477, DLPIDR 0x10000001
   Info : [rp2040.core0] Cortex-M0+ r0p1 processor detected
   Info : [rp2040.core0] target has 4 breakpoints, 2 watchpoints
   Info : [rp2040.core1] Cortex-M0+ r0p1 processor detected
   Info : [rp2040.core1] target has 4 breakpoints, 2 watchpoints
   Info : starting gdb server for rp2040.core0 on 3333
   Info : Listening on port 3333 for gdb connections
   Warn : [rp2040.core1] target was in unknown state when halt was requested
   [rp2040.core0] halted due to debug-request, current mode: Thread
   xPSR: 0xf1000000 pc: 0x000000ee msp: 0x20041f00
   [rp2040.core1] halted due to debug-request, current mode: Thread
   xPSR: 0xf1000000 pc: 0x000000ee msp: 0x20041f00
   ** Programming Started **
   Info : Found flash device 'win w25q16jv' (ID 0x001540ef)
   Info : RP2040 B0 Flash Probe: 2097152 bytes @0x10000000, in 32 sectors

   Info : Padding image section 1 at 0x10022918 with 232 bytes (bank write end alignment)
   Warn : Adding extra erase range, 0x10022a00 .. 0x1002ffff
   ** Programming Finished **
   ** Verify Started **
   ** Verified OK **
   ** Resetting Target **
   shutdown command invoked

.. tip::

   This is the most robust flashing solution if you don't want to physically
   interact with the attached devices every time you want to flash a Pico.

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
attached Pi Pico running an application with USB serial enabled or a Pi Debug
Probe, then use it for testing. To override this behavior, provide a custom
server configuration file with ``--server-config``.

.. code-block:: console

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

.. code-block:: console

   $ gn args out
   # Modify and save the args file to use pw_target_runner.
   pw_targets_ENABLE_RP2040_TEST_RUNNER = true

Step 3: Build changes
=====================
Now, whenever you run ``ninja -C out pi_pico``, all tests affected by changes
since the last build will be rebuilt and then run on the attached device.
Alternatively, you may use ``pw watch`` to set up Pigweed to trigger
builds/tests whenever changes to source files are detected.

-----------------------
Connect with pw_console
-----------------------
Once the board has been flashed, you can connect to it and send RPC commands
via the Pigweed console:

.. code-block:: console

   $ pw-system-console -d /dev/{ttyX} -b 115200 \
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

---------------------
Interactive debugging
---------------------
To interactively debug a Pico, first ensure you are set up for
:ref:`target-raspberry-pi-pico-flashing-using_openocd`\.

In one terminal window, start an OpenOCD GDB server with the following command:

.. code-block:: console

   $ openocd -f interface/cmsis-dap.cfg \
         -f target/rp2040.cfg -c "adapter speed 5000"

In a second terminal window, connect to the open GDB server, passing the binary
you will be debugging:

.. code-block:: console

   $ arm-none-eabi-gdb -ex "target remote :3333" \
     out/rp2040.size_optimized/obj/pw_system/bin/system_example.elf

Helpful GDB commands
====================
+---------------------------------------------------------+--------------------+
| Action                                                  | shortcut / command |
+=========================================================+====================+
| Reset the running device, stopping immediately          | ``mon reset halt`` |
+---------------------------------------------------------+--------------------+
| Continue execution until pause or breakpoint            |              ``c`` |
+---------------------------------------------------------+--------------------+
| Pause execution                                         |         ``ctrl+c`` |
+---------------------------------------------------------+--------------------+
| Show backtrace                                          |             ``bt`` |
+---------------------------------------------------------+--------------------+
