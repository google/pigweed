.. _module-pw_i2c:

------
pw_i2c
------
.. warning::
   This module is under construction, not ready for use, and the documentation
   is incomplete.

pw_i2c contains interfaces and utility functions for using I2C.

Features
========

pw::i2c::Initiator
------------------
.. doxygenclass:: pw::i2c::Initiator
   :members:

pw::i2c::Device
---------------
The common interface for interfacing with generic I2C devices. This object
contains ``pw::i2c::Address`` and wraps the ``pw::i2c::Initiator`` API.
Common use case includes streaming arbitrary data (Read/Write). Only works
with devices with a single device address.

.. note::
   ``Device`` is intended to represent ownership of a specific responder.
   Individual transactions are atomic (as described under ``Initiator``), but
   there is no synchronization for sequences of transactions. Therefore, shared
   access should be faciliated with higher level application abstractions. To
   help enforce this, the ``Device`` object is only movable and not copyable.

pw::i2c::RegisterDevice
-----------------------
The common interface for interfacing with register devices. Contains methods
to help read and write registers from and to the device. Users should have a
understanding of the capabilities of their device such as register address
sizes, register data sizes, byte addressability, bulk transactions, etc in
order to effectively use this interface.

pw::i2c::MockInitiator
----------------------
A generic mocked backend for for pw::i2c::Initiator. This is specifically
intended for use when developing drivers for i2c devices. This is structured
around a set of 'transactions' where each transaction contains a write, read and
a timeout. A transaction list can then be passed to the MockInitiator, where
each consecutive call to read/write will iterate to the next transaction in the
list. An example of this is shown below:

.. code-block:: cpp

  using pw::i2c::Address;
  using pw::i2c::MakeExpectedTransactionArray;
  using pw::i2c::MockInitiator;
  using pw::i2c::WriteTransaction;
  using std::literals::chrono_literals::ms;

  constexpr Address kAddress1 = Address::SevenBit<0x01>();
  constexpr auto kExpectWrite1 = pw::bytes::Array<1, 2, 3, 4, 5>();
  constexpr auto kExpectWrite2 = pw::bytes::Array<3, 4, 5>();
  auto expected_transactions = MakeExpectedTransactionArray(
      {ProbeTransaction(pw::OkStatus, kAddress1, 2ms),
       WriteTransaction(pw::OkStatus(), kAddress1, kExpectWrite1, 1ms),
       WriteTransaction(pw::OkStatus(), kAddress2, kExpectWrite2, 1ms)});
  MockInitiator i2c_mock(expected_transactions);

  // Begin driver code
  Status status = i2c_mock.ProbeDeviceFor(kAddress1, 2ms);

  ConstByteSpan write1 = kExpectWrite1;
  // write1 is ok as i2c_mock expects {1, 2, 3, 4, 5} == {1, 2, 3, 4, 5}
  Status status = i2c_mock.WriteFor(kAddress1, write1, 2ms);

  // Takes the first two bytes from the expected array to build a mismatching
  // span to write.
  ConstByteSpan write2 = pw::span(kExpectWrite2).first(2);
  // write2 fails as i2c_mock expects {3, 4, 5} != {3, 4}
  status = i2c_mock.WriteFor(kAddress2, write2, 2ms);
  // End driver code

  // Optionally check if the mocked transaction list has been exhausted.
  // Alternatively this is also called from MockInitiator::~MockInitiator().
  EXPECT_EQ(mocked_i2c.Finalize(), OkStatus());

pw::i2c::GmockInitiator
-----------------------
gMock of Initiator used for testing and mocking out the Initiator.

I2c Debug Service
=================
This module implements an I2C register access service for debugging and bringup.
To use, provide it with a callback function that returns an ``Initiator`` for
the specified ``bus_index``.

Example invocations
-------------------
Using the pigweed console, you can invoke the service to perform an I2C read:

.. code-block:: python

  device.rpcs.pw.i2c.I2c.I2cRead(bus_index=0, target_address=0x22, register_address=b'\x0e', read_size=1)

The above shows reading register 0x0e on a device located at
I2C address 0x22.

For responders that support 4 byte register width, you can specify as:

.. code-block:: python

  device.rpcs.pw.i2c.I2c.I2cRead(bus_index=0, target_address=<address>, register_address=b'\x00\x00\x00\x00', read_size=4)


And similarly, for performing I2C write:

.. code-block:: python

  device.rpcs.pw.i2c.I2c.I2cWrite(bus_index=0, target_address=0x22,register_address=b'\x0e', value=b'\xbc')


Similarly, multi-byte writes can also be specified with the bytes fields for
`register_address` and `value`.

I2C responders that require multi-byte access may expect a specific endianness.
The order of bytes specified in the bytes field will match the order of bytes
sent/received on the bus. Maximum supported value for multi-byte access is
4 bytes.


.. toctree::
   :hidden:
   :maxdepth: 1

   Backends <backends>
