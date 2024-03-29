.. _module-pw_i2c_rp2040:

-------------
pw_i2c_rp2040
-------------
.. pigweed-module::
   :name: pw_i2c_rp2040

.. pw_i2c_rp2040-example-start

.. code-block:: cpp

   #include "pw_i2c_rp2040/initiator.h"

   #include "hardware/i2c.h"

   constexpr pw::i2c::Rp2040Initiator::Config ki2cConfig{
     .clock_frequency = 400'000,
     .sda_pin = 8,
     .scl_pin = 9,
   };

    pw::i2c::Rp2040Initiator i2c_bus(ki2cConfig, i2c0);
    // Calls these Pico SDK functions:
    // * gpio_set_function(8, GPIO_FUNC_I2C)
    // * gpio_set_function(9, GPIO_FUNC_I2C)
    i2c_bus.Enable();

.. pw_i2c_rp2040-example-end

``pw_i2c_rp2040`` implements the :ref:`module-pw_i2c` interface using the
Raspberry Pi Pico SDK.

The implementation is based on the I2C driver in the Pico SDK. I2C transfers
use the blocking driver API which uses busy waiting under the hood.

Implementation notes
====================
The ``Enable()`` function calls `gpio_set_function
<https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpipc56748afaf477c99958b>`_
from the Pico SDK.

Under the hood this implementation uses the following Pico SDK functions which only
allow specifying timeouts in microseconds:

- `i2c_read_blocking_until <https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip9cd3e6e1aeea56af6388>`_
- `i2c_read_timeout_us <https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip0102e3f420f091f30b00>`_
- `i2c_write_blocking_until <https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip03d01a63251da3cc0588>`_
- `i2c_write_timeout_us <https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip6ca2b36048b95c5e0b07>`_

One additional microsecond is added to each timeout value to ensure reads and
writes wait at least the full duration. Ideally a single clock tick would be
added but that is not currently possible with the Pico SDK APIs.
