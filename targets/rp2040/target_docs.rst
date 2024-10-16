.. _target-rp2040:

===================
Raspberry Pi RP2040
===================
.. _Raspberry Pi RP2040: https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html
.. _Raspberry Pi Pico: https://www.raspberrypi.com/products/raspberry-pi-pico/

This page explains Pigweed's support for the `Raspberry Pi RP2040`_ microcontroller
and for boards built on top of the RP2040, such as the `Raspberry Pi Pico`_.

--------------
Intended usage
--------------
Pigweed's :ref:`mission <docs-mission>` is to help large teams
develop embedded systems sustainably, robustly, and rapidly.
Our support for the RP2040 revolves around making it easier for
these teams to develop complex prototypes or mass market products
on top of the RP2040. Our main goals are to make it easy to:

.. _C/C++ SDK: https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html

* Do your end-to-end development lifecycle (building, flashing, testing, etc.)
  in Bazel.
* Author your embedded system in portable C++. You can write most of your
  system on top of Pigweed's hardware-agnostic :ref:`modules <docs-glossary-module>`.
  If there's anything not covered by Pigweed's modules, you can fallback
  to using the official `C/C++ SDK`_ directly.

.. _MicroPython SDK: https://www.raspberrypi.com/documentation/microcontrollers/micropython.html

If you're building relatively simple stuff solely for the RP2040 and just want
to get everything working very quickly and easily, then Pigweed probably won't
be a good fit for you. You'll probably be happier with the official
`MicroPython SDK`_ or the official `C/C++ SDK`_.

--------
Examples
--------
.. _Sense showcase: https://pigweed.googlesource.com/pigweed/showcase/sense/

The `Sense showcase`_ is our work-in-progress demonstration of using
the RP2040, Bazel, and Pigweed together. The showcase is scheduled to be
completed in July 2024.

-------
Modules
-------
Most Pigweed :ref:`modules <docs-glossary-module>` work on all
hardware platforms. A few areas such as I2C require integrating with
a specific Pigweed module.

.. csv-table::
   :header: "Description", "Module"

   "Time primitives", ":ref:`module-pw_chrono_rp2040`"
   "GPIO (digital I/O)", ":ref:`module-pw_digital_io_rp2040`"
   "I2C", ":ref:`module-pw_i2c_rp2040`"
   "SPI", ":ref:`module-pw_spi_rp2040`"
   "Basic I/O for bringup and debugging", ":ref:`module-pw_sys_io_rp2040`"

.. toctree::
   :maxdepth: 1
   :hidden:

   Upstream Pigweed <upstream>
