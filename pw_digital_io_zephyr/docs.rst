.. _module-pw_digital_io_zephyr:

====================
pw_digital_io_zephyr
====================
.. pigweed-module::
   :name: pw_digital_io_zephyr

``pw_digital_io_zephyr`` implements the :ref:`module-pw_digital_io` interface
using the `Zephyr API <https://github.com/zephyrproject-rtos/zephyr>`_.

Setup
=====
Use of this module requires setting up the Zephyr build environment for use with
Pigweed. Follow steps in :ref:`docs-quickstart-zephyr` to get setup.

Examples
========
Used devicetree to initialize any of the digital IO classes. Assume the
following devicetree snippet:

.. code-block:: devicetree

   my_node: node@20 {
     compatible = "org,foo-bar";
     gpios = <&gpio0 0 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>
   };

Then in your application:

.. code-block:: cpp

   #include "pw_digital_io_zephyr/digital_io.h"
   #include "zephyr/gpio.h"

   constexpr pw::digital_io::ZephyrDigitalInInterrupt kNodeGpio(
       GPIO_DT_SPEC_GET(DT_NODELABEL(my_node), gpios));
