.. _docs-quickstart-zephyr:

=================
Zephyr quickstart
=================
.. _Zephyr: https://zephyrproject.org/
.. _native_sim: https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html
.. _GPIO Driver API: https://docs.zephyrproject.org/latest/doxygen/html/group__devicetree-gpio.html

This tutorial shows you how to set up a new C++-based `Zephyr`_ project that's
ready to use Pigweed. You'll learn how to build and run the project's app on
`native_sim`_ as well as a physical Raspberry Pi Pico. The app uses
:ref:`module-pw_log` and :ref:`module-pw_string` to log messages and
Zephyr's `GPIO Driver API`_ to blink an LED.

.. figure:: https://storage.googleapis.com/pigweed-media/zephyr-quickstart-pw_ide.png
   :alt: Editing the Zephyr quickstart project in VS Code

   The project's :ref:`module-pw_ide` integration provides code intelligence
   and easy target swapping in VS Code

.. _docs-quickstart-zephyr-prereqs:

-------------
Prerequisites
-------------
* **Disk space**: After setting everything up, the project takes ~19GB of space.
  The project clones the Zephyr and Pigweed repos as well as their dependencies.
  It also downloads toolchains and sets up a hermetic development environment.
* **Operating systems**: This tutorial has only been validated on Debian-based
  Linux and macOS. Windows support is a work in progress.

.. _docs-quickstart-zephyr-setup:

-----
Setup
-----
#. Complete Pigweed's :ref:`First-time setup <docs-first-time-setup-guide>`
   process.

#. Clone the starter repo.

   .. tab-set::

      .. tab-item:: Linux
         :sync: lin

         .. code-block:: console

            git clone --recursive \
              https://pigweed.googlesource.com/pigweed/quickstart/zephyr \
              ~/zephyr-quickstart

      .. tab-item:: macOS
         :sync: mac

         .. code-block:: console

            git clone --recursive \
              https://pigweed.googlesource.com/pigweed/quickstart/zephyr \
              ~/zephyr-quickstart

   .. _main Pigweed: https://pigweed.googlesource.com/pigweed/pigweed/
   .. _main Zephyr: https://github.com/zephyrproject-rtos/zephyr

   This command downloads the `main Pigweed`_ and `main Zephyr`_ repos
   as Git submodules.

#. Change your working directory to the quickstart repo.

   .. tab-set::

      .. tab-item:: Linux
         :sync: lin

         .. code-block:: console

            cd ~/quickstart-zephyr

      .. tab-item:: macOS
         :sync: mac

         .. code-block:: console

            cd ~/quickstart-zephyr

#. Bootstrap the repo.

   .. tab-set::

      .. tab-item:: Linux
         :sync: lin

         .. code-block:: console

            source bootstrap.sh

      .. tab-item:: macOS
         :sync: mac

         .. code-block:: console

            source bootstrap.sh

   Pigweed's bootstrap workflow creates a hermetic development environment
   for you, including toolchain setup!

   .. tip::

      For subsequent development sessions, activate your development environment
      with the following command:

      .. tab-set::

         .. tab-item:: Linux
            :sync: lin

            .. code-block:: console

               source activate.sh

         .. tab-item:: macOS
            :sync: mac

            .. code-block:: console

               source activate.sh

      The activate script is faster than the bootstrap script. You only need to
      run the bootstrap script after updating your Zephyr or Pigweed submodules.

   .. _West: https://docs.zephyrproject.org/latest/develop/west/index.html

#. Initialize your `West`_ workspace using the manifest that came with the
   starter repo.

   .. code-block:: console

      west init -l manifest

#. Update your West workspace.

   .. code-block:: console

      west update

#. (Optional) Initialize :ref:`module-pw_ide` if you plan on working in
   VS Code. ``pw_ide`` provides code intelligence features and makes it
   easy to swap targets.

   .. code-block:: console

      pw ide sync

.. _docs-quickstart-zephyr-build:

---------------------
Build and run the app
---------------------

.. _docs-quickstart-zephyr-build-native_sim:

Native simulator
================
#. Build the quickstart app for `native_sim`_ and run it:

   .. code-block:: console

      export ZEPHYR_TOOLCHAIN_VARIANT=llvm &&
        west build -p -b native_sim app -t run

   You should see the app successfully build and then log messages to
   ``stdout``:

   .. code-block:: none

      [00:00:00.000,000] <inf> pigweed:  Hello, world!
      [00:00:00.000,000] <inf> pigweed:  LED state: OFF
      [00:00:01.010,000] <inf> pigweed:  LED state: ON
      [00:00:02.020,000] <inf> pigweed:  LED state: OFF
      [00:00:03.030,000] <inf> pigweed:  LED state: ON
      [00:00:04.040,000] <inf> pigweed:  LED state: OFF

   .. important::

      When building for ``native_sim`` make sure that
      ``ZEPHYR_TOOLCHAIN_VARIANT`` is set to ``llvm``.
      See :ref:`docs-quickstart-zephyr-troubleshooting-envvar`.

#. (Optional) Build and run an upstream Zephyr sample app:

   .. code-block:: console

      west build -p -b native_sim third_party/zephyr/samples/basic/blinky -t run

.. _docs-quickstart-zephyr-build-pico:

Raspberry Pi Pico
=================
.. _Raspberry Pi Pico: https://docs.zephyrproject.org/latest/boards/raspberrypi/rpi_pico/doc/index.html
.. _Pico SDK: https://github.com/raspberrypi/pico-sdk
.. _picotool: https://github.com/raspberrypi/picotool

#. Build the quickstart app for `Raspberry Pi Pico`_:

   .. code-block:: console

      export ZEPHYR_TOOLCHAIN_VARIANT=zephyr &&
        west build -p -b rpi_pico app

   .. important::

      When building for physical boards make sure that
      ``ZEPHYR_TOOLCHAIN_VARIANT`` is set to ``zephyr``.
      See :ref:`docs-quickstart-zephyr-troubleshooting-envvar`.

#. Install the `Pico SDK`_ and `picotool`_ so that you can easily
   flash the quickstart app onto your Pico over USB without needing to
   manually put your Pico board into ``BOOTSEL`` mode:

   .. code-block:: console

      pw package install pico_sdk
      pw package install picotool

#. Put the following rules into ``/usr/lib/udev/rules.d/49-picoprobe.rules``:

   .. code-block:: none

      # Pico app mode
      SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", MODE:="0666"
      KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", MODE:="0666", SYMLINK+="rp2040"
      # RP2 Boot
      SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE:="0666"
      KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE:="0666", SYMLINK+="rp2040"
      # Picoprobe
      SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0004", MODE:="0666"
      KERNEL=="ttyACM*", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0004", MODE:="0666", SYMLINK+="picoprobe"

#. Apply the rules:

   .. code-block:: console

      sudo udevadm control --reload-rules
      sudo udevadm trigger

#. Flash the app onto your board:

   .. code-block:: console

      picotool reboot -f -u &&
        sleep 3 &&
        picotool load -x ./build/zephyr/zephyr.elf

.. _docs-quickstart-zephyr-troubleshooting:

---------------
Troubleshooting
---------------

.. _docs-quickstart-zephyr-troubleshooting-envvar:

``fatal error: bits/c++config.h: No such file or directory``
============================================================
If you see a compilation error about not being able to find
``<bits/c++config.h>``, make sure that your ``ZEPHYR_TOOLCHAIN_VARIANT``
environment variable is correctly set:

* Set it to ``llvm`` when building for ``native_sim``.
* Set it to ``zephyr`` when building for physical boards.

Here's an example of the error:

.. code-block:: console

   ...
   [2/109] Generating include/generated/version.h
   -- Zephyr version: 3.6.99 (~/zephyr-quickstart/third_party/zephyr), build: v3.6.0-1976-g8a88cd4805b0
   [10/109] Building CXX object modules/pigweed/pw_string/CMakeFiles/pw_string.to_string.dir/type_to_string.cc.obj
   FAILED: modules/pigweed/pw_string/CMakeFiles/pw_string.to_string.dir/type_to_string.cc.obj
   ccache /usr/bin/g++
   ...
   -c ~/zephyr-quickstart/third_party/pigweed/pw_string/type_to_string.cc
   In file included from ~/zephyr-quickstart/third_party/pigweed/pw_string/public/pw_string/type_to_string.h:20,
                    from ~/zephyr-quickstart/third_party/pigweed/pw_string/type_to_string.cc:15:
   /usr/include/c++/13/cstdint:38:10: fatal error: bits/c++config.h: No such file or directory
      38 | #include <bits/c++config.h>
         |          ^~~~~~~~~~~~~~~~~~
   compilation terminated.
   ...
   [12/109] Building CXX object modules/pigweed/pw_string/CMakeFiles/pw_string.builder.dir/string_builder.cc.obj
   FAILED: modules/pigweed/pw_string/CMakeFiles/pw_string.builder.dir/string_builder.cc.obj
   ccache /usr/bin/g++
   ...
   -c ~/zephyr-quickstart/third_party/pigweed/pw_string/string_builder.cc
   In file included from /usr/include/c++/13/algorithm:60,
                    from ~/zephyr-quickstart/third_party/pigweed/pw_string/public/pw_string/string_builder.h:21,
                    from ~/zephyr-quickstart/third_party/pigweed/pw_string/string_builder.cc:15:
   /usr/include/c++/13/bits/stl_algobase.h:59:10: fatal error: bits/c++config.h: No such file or directory
      59 | #include <bits/c++config.h>
         |          ^~~~~~~~~~~~~~~~~~
   compilation terminated.
   [15/109] Building C object zephyr/CMakeFiles/offsets.dir/arch/posix/core/offsets/offsets.c.obj
   ninja: build stopped: subcommand failed.
   FATAL ERROR: command exited with status 1: ~/zephyr-quickstart/environment/cipd/packages/cmake/bin/cmake
     --build ~/zephyr-quickstart/build --target run

