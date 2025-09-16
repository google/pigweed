.. _module-pw_i2c-quickstart-and-guides:

=====================
Quickstart and guides
=====================
.. pigweed-module-subpage::
   :name: pw_i2c

.. _module-pw_i2c-quickstart:

----------
Quickstart
----------
.. tab-set::

   .. tab-item:: Bazel

      Depend on the ``pw_i2c`` API and an appropriate implementation of the
      API like ``pw_i2c_linux``. See :ref:`module-pw_i2c-impl` for the
      list of existing implementations and to learn how to create your own.

      .. code-block:: python

         cc_library(
           # ...
           deps = [
             # ...
             "@pigweed//pw_i2c:address",
             "@pigweed//pw_i2c:device",
             # ...
           ] + select({
             "@platforms//os:linux": [
               "@pigweed//pw_i2c_linux:initiator",
             ],
             "//conditions:default": [
               # Fake example of a custom implementation.
               "//lib/pw_i2c_my_device:initiator",
             ],
           }),
         )

      If creating your own implementation, depend on the virtual interface:

      .. code-block:: python

         cc_library(
           name = "initiator",
           srcs = ["initiator.cc"],
           hdrs = ["initiator.h"],
           deps = ["@pigweed//pw_i2c:initiator"],
         )

Write some C++ code to interact with an I2C device:

.. include:: ../pw_i2c_rp2040/docs.rst
   :start-after: .. pw_i2c_rp2040-example-start
   :end-before: .. pw_i2c_rp2040-example-end

.. _module-pw_i2c-guides:

------
Guides
------

API overview
============
* :doxylink:`pw::i2c::Address` is a helper class for representing I2C
  addresses.
* :doxylink:`pw::i2c::Message` is a helper class for representing individual
  read and write components within a single i2c transaction.
* :doxylink:`pw::i2c::Initiator` is the common, base driver interface for
  communicating with I2C devices.
* :doxylink:`pw::i2c::Device` is a helper class that takes a reference
  to an :doxylink:`pw::i2c::Initiator` instance and provides easier access
  to a single I2C device.
* :doxylink:`pw::i2c::RegisterDevice` extends :doxylink:`pw::i2c::Device`
  for easier access to a single I2C device's registers.
* :doxylink:`pw::i2c::I2cService` is a service for performing I2C
  transactions over RPC.
* :doxylink:`pw::i2c::MockMessageInitiator` is a generic mocked backend for
  :doxylink:`pw::i2c::Initiator`. It accepts multiple
  :doxylink:`pw::i2c::MockMessageTransaction`, each of which is mock
  transmitted as one bus transaction.
* :doxylink:`pw::i2c::MockMessageTransaction` represents a test i2c
  transaction. Each transaction consists of an arbitrary sequence of
  :doxylink:`pw::i2c::MockMessage` objects that are transmitted in one bus
  operation.
* :doxylink:`pw::i2c::MockMessage` represents one read or write element of
  an i2c transaction.

.. _module-pw_i2c-guides-mock:

Mock I2C transactions
=====================
See the example in :doxylink:`pw::i2c::MockInitiator`.

.. _module-pw_i2c-guides-registerdevice:

Configure and read an I2C device's registers
============================================
.. code-block:: c++

   #include <chrono>
   #include <cstddef>
   #include <cstdint>

   #include "pw_bytes/bit.h"
   #include "pw_i2c/address.h"
   #include "pw_i2c/register_device.h"
   #include "pw_log/log.h"
   #include "pw_status/status.h"

   using ::pw::Status;
   using namespace std::chrono_literals;

   // Search for `pi4ioe5v6416` in the Kudzu codebase to see real usage of
   // pw::i2c::RegisterDevice
   namespace pw::pi4ioe5v6416 {

   namespace {

   constexpr pw::i2c::Address kAddress = pw::i2c::Address::SevenBit<0x20>();
   enum Register : uint32_t {
     InputPort0 = 0x0,
     ConfigPort0 = 0x6,
     PullUpDownEnablePort0 = 0x46,
     PullUpDownSelectionPort0 = 0x48,
   };

   }  // namespace

   // This particular example instantiates `pw::i2c::RegisterDevice`
   // as part of a higher-level general "device" interface.
   // See //lib/pi4ioe5v6416/public/pi4ioe5v6416/device.h in Kudzu.
   Device::Device(pw::i2c::Initiator& initiator)
       : initiator_(initiator),
         register_device_(initiator,
                          kAddress,
                          endian::little,
                          pw::i2c::RegisterAddressSize::k1Byte) {}

   Status Device::Enable() {
     // Set port 0 as inputs for buttons (1=input)
     device_.WriteRegister8(
         Register::ConfigPort0, 0xff, pw::chrono::SystemClock::for_at_least(10ms));
     // Select pullup resistors for button input (1=pullup)
     device_.WriteRegister8(Register::PullUpDownSelectionPort0,
                            0xff,
                            pw::chrono::SystemClock::for_at_least(10ms));
     // Enable pullup/down resistors for button input (1=enable)
     device_.WriteRegister8(Register::PullUpDownEnablePort0,
                            0xff,
                            pw::chrono::SystemClock::for_at_least(10ms));
     return OkStatus();
   }

   pw::Result<uint8_t> Device::ReadPort0() {
     return device_.ReadRegister8(Register::InputPort0,
                                  pw::chrono::SystemClock::for_at_least(10ms));
   }

   }  // namespace pw::pi4ioe5v6416

The code example above was adapted from :ref:`docs-kudzu`. See the following
files for real ``pw::i2c::RegisterDevice`` usage:

* `//lib/pi4ioe5v6416/device.cc <https://pigweed.googlesource.com/pigweed/kudzu/+/refs/heads/main/lib/pi4ioe5v6416/device.cc>`_
* `//lib/pi4ioe5v6416/public/pi4ioe5v6416/device.h <https://pigweed.googlesource.com/pigweed/kudzu/+/refs/heads/main/lib/pi4ioe5v6416/public/pi4ioe5v6416/device.h>`_

.. _module-pw_i2c-guides-rpc:

Access an I2C device's registers over RPC
=========================================
:doxylink:`pw::i2c::I2cService` enables accessing an I2C device's registers
over RPC.

Using :ref:`module-pw_console`, invoke the service to perform an I2C read:

.. code-block:: python

   # Read register `0x0e` from the device at `0x22`.
   device.rpcs.pw.i2c.I2c.I2cRead(
       bus_index=0,
       target_address=0x22,
       register_address=b'\x0e',
       read_size=1
   )

For responders that support 4-byte register width, you can specify the register
address like this:

.. code-block:: python

   device.rpcs.pw.i2c.I2c.I2cRead(
       bus_index=0,
       target_address=<address>,
       register_address=b'\x00\x00\x00\x00',
       read_size=4
   )

To perform an I2C write:

.. code-block:: python

   device.rpcs.pw.i2c.I2c.I2cWrite(
       bus_index=0,
       target_address=0x22,
       register_address=b'\x0e',
       value=b'\xbc'
   )

Multi-byte writes can also be specified with the bytes fields for
``register_address`` and ``value``.

I2C responders that require multi-byte access may expect a specific endianness.
The order of bytes specified in the bytes field will match the order of bytes
sent or received on the bus. The maximum supported value for multi-byte access
is 4 bytes.

.. note::
   :doxylink:`pw::i2c::I2cService` currently only supports 7-bit i2c
   addressing.

Adding a Responder Backend
==========================
To add a new backend for the ``Responder`` API, you must provide a concrete
subclass of ``pw::i2c::Responder``.

A critical part of creating a new backend is testing. ``pw_i2c`` includes a
generic test suite in ``pw_i2c/responder_test.cc`` that can be used to validate
your implementation. To enable this for your backend, you must:

1. **Implement the native test interface:** Create a class named
   ``NativeResponderTest`` that inherits from
   ``pw::i2c::test::NativeResponderTestInterface`` (from
   ``pw_i2c/responder_test_interface.h``).

2. **Configure the test build:** Set the ``pw_i2c_responder_test`` build
   target to use your new backend. The test suite will then run against your
   implementation to ensure it conforms to the API contract.
