.. _module-pw_i2c_linux:

============
pw_i2c_linux
============
.. pigweed-module::
   :name: pw_i2c_linux

``pw_i2c_linux`` implements the ``pw_i2c`` interface using the Linux userspace
``i2c-dev`` driver. Transfers are executed using blocking ``ioctl`` calls.
Arbitrary-order read and write messages are supported by one underlying system
call. A retry mechanism is used to support bus arbitration between multiple
controllers.

Support for ten-bit addresses and the no-start flag is enabled but depends
on underlying support by the linux-managed device.

-------------
API reference
-------------
Moved: :cc:`pw_i2c_linux`

--------
Examples
--------
A simple example illustrating the usage:

.. code-block:: C++

   #include "pw_i2c/address.h"
   #include "pw_i2c/device.h"
   #include "pw_i2c_linux/initiator.h"
   #include "pw_log/log.h"
   #include "pw_result/result.h"

   constexpr auto kBusPath = "/dev/i2c-0";
   constexpr auto kAddress = pw::i2c::Address::SevenBit<0x42>();

   pw::Result<int> result = pw::i2c::LinuxInitiator::OpenI2cBus(kBusPath);
   if (!result.ok()) {
     PW_LOG_ERROR("Failed to open I2C bus [%s]", kBusPath);
     return result.status();
   }
   pw::i2c::LinuxInitiator initiator(*result);
   pw::i2c::Device device(initiator, address);
   // Use device to talk to address.

In real-world use cases, you may want to create an initiator singleton. This
can be done by initializing a function-local static variable with a lambda:

.. code-block:: C++

   #include <functional>

   #include "pw_i2c/address.h"
   #include "pw_i2c/device.h"
   #include "pw_i2c/initiator.h"
   #include "pw_i2c_linux/initiator.h"
   #include "pw_log/log.h"
   #include "pw_result/result.h"
   #include "pw_status/status.h"

   // Open the I2C bus and return an initiator singleton.
   pw::i2c::Initiator* GetInitiator() {
     static constexpr auto kBusPath = "/dev/i2c-0";
     static auto* initiator = std::invoke([]() -> pw::i2c::Initiator* {
       pw::Result<int> result = pw::i2c::LinuxInitiator::OpenI2cBus(kBusPath);
       if (!result.ok()) {
         PW_LOG_ERROR("Failed to open I2C bus [%s]", kBusPath);
         return nullptr;
       }
       return new pw::i2c::Initiator(*result);
     });
     return initiator;
   }

   // Use the initiator from anywhere.
   constexpr auto kAddress = pw::i2c::Address::SevenBit<0x42>();
   auto* initiator = GetInitiator();
   if (initiator == nullptr) {
     PW_LOG_ERROR("I2C initiator unavailable");
     return pw::Status::Internal();
   }
   pw::i2c::Device device(*initiator, address);
   // Use device to talk to address.

.. _module-pw_i2c_linux-cli:

----------------------
Command-line interface
----------------------
This module also provides a tool also named ``pw_i2c_linux_cli`` which
provides a basic command-line interface to the library.

Usage:

.. code-block:: none

   Usage: pw_i2c_linux_cli -D DEVICE -A|-a ADDR [flags]

   Required flags:
     -A/--addr10   Target address, 0x prefix allowed (10-bit i2c extension)
     -a/--address  Target address, 0x prefix allowed (7-bit standard i2c)
     -D/--device   I2C device path (e.g. /dev/i2c-0)

   Optional flags:
     -h/--human    Human-readable output (default: binary, unless output to stdout tty)
     -i/--input    Input file, or - for stdin
                   If not given, no data is sent.
     -l/--lsb      LSB first (default: MSB first)
     -o/--output   Output file (default: stdout)
     -r/--rx-count Number of bytes to receive (defaults to size of input)

Example:

.. code-block:: none

   # Read register 0x0950 (Write two bytes then read one byte)
   $ echo -en "\\x9\\x50" | pw_i2c_linux_cli -D /dev/i2c-2 -a 0x09 -i - -r 1
