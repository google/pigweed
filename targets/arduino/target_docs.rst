.. _target-arduino:

-------
Arduino
-------

This target supports building Pigweed on a few Arduino cores.

.. seealso::
   There are a few caveats when running Pigweed on top of the Arduino API. See
   :ref:`module-pw_arduino_build` for details.

Supported Boards
================

Currently only Teensy 4.x and 3.x boards are supported.

+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| Core                                                             | Board Name                                                        | Compiling | Flashing | Test Runner |
+==================================================================+===================================================================+===========+==========+=============+
| `teensy <https://www.pjrc.com/teensy/td_download.html>`_         | `Teensy 4.1 <https://www.pjrc.com/store/teensy41.html>`_          | ✓         | ✓        |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `teensy <https://www.pjrc.com/teensy/td_download.html>`_         | `Teensy 4.0 <https://www.pjrc.com/store/teensy40.html>`_          | ✓         | ✓        |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `teensy <https://www.pjrc.com/teensy/td_download.html>`_         | `Teensy 3.6 <https://www.pjrc.com/store/teensy36.html>`_          | ✓         | ✓        |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `teensy <https://www.pjrc.com/teensy/td_download.html>`_         | `Teensy 3.5 <https://www.pjrc.com/store/teensy35.html>`_          | ✓         | ✓        |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `teensy <https://www.pjrc.com/teensy/td_download.html>`_         | `Teensy 3.2 <https://www.pjrc.com/store/teensy32.html>`_          | ✓         | ✓        |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `arduino-samd <https://github.com/arduino/ArduinoCore-samd>`_    | `Arduino Zero <https://store.arduino.cc/usa/arduino-zero>`_       |           |          |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `arduino-sam <https://github.com/arduino/ArduinoCore-sam>`_      | `Arduino Due <https://store.arduino.cc/usa/due>`_                 |           |          |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `adafruit-samd <https://github.com/adafruit/ArduinoCore-samd>`_  | `Adafruit Feather M0 <https://www.adafruit.com/?q=feather+m0>`_   |           |          |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `adafruit-samd <https://github.com/adafruit/ArduinoCore-samd>`_  | `Adafruit SAMD51 Boards <https://www.adafruit.com/category/952>`_ |           |          |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+
| `stm32duino <https://github.com/stm32duino/Arduino_Core_STM32>`_ |                                                                   |           |          |             |
+------------------------------------------------------------------+-------------------------------------------------------------------+-----------+----------+-------------+

Setup
=====

You must first install an Arduino core or let Pigweed know where you have cores
installed using the ``dir_pw_third_party_arduino`` and ``arduino_package_path``
build arguments.

Installing Arduino Cores
------------------------

The ``arduino_builder`` utility can install Arduino cores automatically. It's
recommended to install them to into ``third_party/arduino/cores/``.

.. code:: sh

  # Setup pigweed environment.
  $ source activate.sh
  # Install an arduino core
  $ arduino_builder install-core --prefix ./third_party/arduino/cores/ --core-name teensy

Building
========
To build for this Pigweed target, simply build the top-level "arduino" Ninja
target. You can set Arduino build options using ``gn args out`` or by running:

.. code:: sh

  $ gn gen out --args='dir_pw_third_party_arduino="//third_party/arduino"
                       arduino_package_path="third_party/arduino/cores/teensy"
                       arduino_package_name="teensy/avr"
                       arduino_board="teensy40"
                       arduino_menu_options=["menu.usb.serial", "menu.keys.en-us"]'

Then build with:

.. code:: sh

  $ ninja -C out arduino

To see supported boards and Arduino menu options for a given core:

.. code:: sh

  $ arduino_builder --arduino-package-path ./third_party/arduino/cores/teensy \
                    --arduino-package-name teensy/avr \
                    list-boards
  # Board Name  Description
  # teensy41    Teensy 4.1
  # teensy40    Teensy 4.0
  # teensy36    Teensy 3.6
  # teensy35    Teensy 3.5
  # teensy31    Teensy 3.2 / 3.1

  $ arduino_builder --arduino-package-path ./third_party/arduino/cores/teensy \
                    --arduino-package-name teensy/avr \
                    list-menu-options --board teensy40
  # All Options
  # ----------------------------------------------------------------
  # menu.usb.serial             Serial
  # menu.usb.serial2            Dual Serial
  # menu.usb.serial3            Triple Serial
  # menu.usb.keyboard           Keyboard
  # menu.usb.touch              Keyboard + Touch Screen
  # menu.usb.hidtouch           Keyboard + Mouse + Touch Screen
  # menu.usb.hid                Keyboard + Mouse + Joystick
  # menu.usb.serialhid          Serial + Keyboard + Mouse + Joystick
  # menu.usb.midi               MIDI
  # ...
  #
  # Default Options
  # --------------------------------------
  # menu.usb.serial             Serial
  # menu.speed.600              600 MHz
  # menu.opt.o2std              Faster
  # menu.keys.en-us             US English

Testing
=======
When working in upstream Pigweed, building this target will build all Pigweed
modules' unit tests.  These tests can be run on-device in a few different ways.

Run a unit test
---------------
If using ``out`` as a build directory, tests will be located in
``out/arduino_debug/obj/[module name]/[test_name].elf``.

For now these tests must be flashed manually on device. Here is a sample bash
script to run all tests on a Linux machine.

.. code:: sh

  #!/bin/bash
  gn gen out --export-compile-commands \
      --args='dir_pw_third_party_arduino="//third_party/arduino"
              arduino_core_name="teensy"
              arduino_package_name="teensy/avr"
              arduino_board="teensy40"
              arduino_menu_options=["menu.usb.serial", "menu.keys.en-us"]' && \
    ninja -C out arduino

  SERIAL_PORT=/dev/ttyACM0
  for f in $(find out/arduino_debug/obj/ -iname "*.elf"); do
      BUILD_PATH=$(dirname $f)
      PROJECT_NAME=$(basename -s .elf $f)
      COMMON_ARGS="--quiet --arduino-package-path ./third_party/arduino/cores/teensy
                   --arduino-package-name teensy/avr
                   --compiler-path-override ./.environment/cipd/pigweed/bin
                   show
                   --build-path ${BUILD_PATH} --build-project-name ${PROJECT_NAME}
                   --board teensy40 --menu-options menu.usb.serial menu.keys.en-us"
      echo "==> OBJCOPY" $f
      arduino_builder $COMMON_ARGS --run-objcopy
      # Optional
      # arduino_builder $COMMON_ARGS --run-postbuild
      echo "==> FLASH" $f
      arduino_builder --serial-port $SERIAL_PORT $COMMON_ARGS --run-upload-command teensyloader
      while true; do
          sleep .1; ls $SERIAL_PORT 2>/dev/null && break
      done
      python3 -m serial.tools.miniterm $SERIAL_PORT 115200
  done
