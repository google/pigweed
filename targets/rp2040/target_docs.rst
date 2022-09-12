.. _target-raspberry-pi-pico:

-----------------
Raspberry Pi Pico
-----------------
.. warning::
  This target is in an early state and is under active development. Usability
  is not very polished, and many features/configuration options that work in
  upstream Pi Pico CMake build have not yet been ported to the GN build.

Setup
=====
To use this target, Pigweed must be set up to build against the Raspberry Pi
Pico SDK. This can be downloaded via ``pw package``, and then the build must be
manually configured to point to the location of the downloaded SDK.

.. code:: sh

  pw package install pico_sdk

  gn args out
    # Add this line.
    PICO_SRC_DIR = getenv("PW_PACKAGE_ROOT") + "/pico_sdk"

Usage
=====
The Pi Pico is currently configured to output logs and test results over UART
via GPIO 1 and 2 (TX and RX, respectively) at a baud rate of 115200. Because
of this, you'll need a USB TTL adapter to communicate with the Pi Pico.

Once the pico SDK is configured, the Pi Pico will build as part of the default
GN build:

.. code:: sh

  ninja -C out

Pigweed's build will produce ELF and UF2 files for each unit test built for the
Pi Pico.

Flashing
========
Flashing the Pi Pico is two easy steps:

#. While holding the button on the Pi Pico, connect the Pico to your computer
   via the micro USB port.
#. Copy the desired UF2 firmware image to the RPI-RP2 volume that enumerated
   when you connected the Pico.
