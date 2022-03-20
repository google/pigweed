.. _target-raspberry-pi-pico:

-----------------
Raspberry Pi Pico
-----------------
.. warning::
  This target is in an early state and is under active development. Usability
  is not very polished, and many features/configuration options that work in
  upstream Pi Pico CMake build have not yet been ported to the GN build.

-----
Setup
-----
To use this target, Pigweed must be set up to build against the Raspberry Pi
Pico SDK. This can be downloaded via ``pw package``, and then the build must be
manually configured to point to the location of the downloaded SDK.

.. code:: sh

  pw package install pico_sdk

  gn args out
    # Add these lines, replacing ${PW_ROOT} with the path to the location that
    # Pigweed is checked out at.
    PICO_SRC_DIR = "${PW_ROOT}/.environment/packages/pico_sdk"

-----
Usage
-----
The Pi Pico is currently configured to output logs and test results over UART
via GPIO 1 and 2 (TX and RX, respectively) at a baud rate of 115200. Because
of this, you'll need a USB TTL adapter to communicate with the Pi Pico.

Once the pico SDK is configured, the Pi Pico will build as part of the default
GN build:

.. code:: sh

  ninja -C out

Pigweed's build will produce ELF files for each unit test built for the Pi Pico.
While ELF files can be flashed to a Pi Pico via SWD, it's slightly easier to
use the Pi Pico's bootloader to flash the firmware as a UF2 file.

Pigweed currently does not yet build/provide the elf2uf2 utility used to convert
ELF files to UF2 files. This tool can be built from within the Pi Pico SDK with
the following command:

.. code:: sh

  mkdir build && cd build && cmake -G Ninja ../ && ninja
  # Copy the tool so it's visible in your PATH.
  cp elf2uf2/elf2uf2 $HOME/bin/elf2uf2

Flashing
========
Flashing the Pi Pico is as easy as 1-2-3:

#. Create a UF2 file from an ELF file using ``elf2uf2``.
#. While holding the button on the Pi Pico, connect the Pico to your computer
   via the micro USB port.
#. Copy the UF2 to the RPI-RP2 volume that enumerated when you connected the
   Pico.
