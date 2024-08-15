.. _module-pw_display:

==========
pw_display
==========
.. pigweed-module::
   :name: pw_display

.. attention::
   This module's initial implementation is informed by SEED :ref:`seed-0104`,
   but is being reevaluated as it is migrated from `Pigweed's experimental
   repsitory <https://pigweed.googlesource.com/pigweed/experimental/>`_.

---------
Libraries
---------

Color
-----
.. seealso::
   Color API: :ref:`module-pw_display-api-color`

The color library defines base pixel format types and the `ColorRgba` class for
converting between various types.

Display controllers often support a variety of data formats for representing a
single pixel. For example:

256 color grayscale:
    8 bits per pixel for a total of 256 shades of gray
4k color: RGB444
    12 bits total; 4 bits for each color: red, green, blue
65k color: RGB565
    16 bits total; 5 bits for red, 6 bits for green and 5 bits for blue
262k color: RGB666
    18 bits total; 6 bits for each color: red, green, blue
16.7M color: RGB888
    24 bits total; 8 bits for each color: red, green, blue

.. note::
   ``pw_display`` drawing libraries will initially only operate on RGB565 pixels
   for a few reasons:

   - 16 bits per color is easily represented as a single 16 bit unsigned
     integer. No special framebuffer data packing logic is needed for 100%
     memory utilization.
   - RGB565 is 65k color which is a good compromise on color fidelity and memory
     footprint.
   - RGB565 has wide support by common display controllers used in the embedded
     space.

.. toctree::
   :hidden:
   :maxdepth: 1

   api
