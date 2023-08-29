.. _module-pw_i2c_linux:

---------------------
pw_i2c_linux
---------------------
``pw_i2c_linux`` implements the ``pw_i2c`` interface using the Linux userspace
``i2c-dev`` driver. Transfers are executed using blocking ``ioctl`` calls.
Write+read transactions are implemented atomically using a single system call,
and a retry mechanism is used to support bus arbitration between multiple
controllers.

C++
===
.. doxygenclass:: pw::i2c::LinuxInitiator
   :members:

Examples
========
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

Caveats
=======
Only 7-bit addresses are supported right now, but it should be possible to add
support for 10-bit addresses with minimal changes - as long as the Linux driver
supports 10-bit addresses.
