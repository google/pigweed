.. _module-pw_spi:

======
pw_spi
======
.. pigweed-module::
   :name: pw_spi

``pw_spi`` provides a set of interfaces for communicating with Serial
Peripheral Interface (SPI) responders attached to a target. It also provides an
interface for implementing SPI responders.

--------
Overview
--------
The ``pw_spi`` module provides a series of interfaces that facilitate the
development of SPI responder drivers that are abstracted from the target's
SPI hardware implementation. The interface consists of these main classes:

- :doxylink:`Initiator <pw::spi::Initiator>` - Interface for configuring a SPI
  bus, and using it to transmit and receive data.
- :doxylink:`ChipSelector <pw::spi::ChipSelector>` - Interface for
  enabling/disabling a SPI responder attached to the bus.
- :doxylink:`Device <pw::spi::Device>` - primary HAL interface used to interact
  with a SPI responder.
- :doxylink:`Responder <pw::spi::Responder>` - Interface for implementing a SPI
  responder.

``pw_spi`` relies on a target-specific implementations of :doxylink:`Initiator
<pw::spi::Initiator>` and :doxylink:`ChipSelector <pw::spi::ChipSelector>` to
be defined, and injected into :doxylink:`Device <pw::spi::Device>` objects
which are used to communicate with a given responder attached to a target's SPI
bus.

--------
Examples
--------

Constructing a SPI device
=========================
.. code-block:: cpp

   constexpr pw::spi::Config kConfig = {
       .polarity = pw::spi::ClockPolarity::kActiveHigh,
       .phase = pw::spi::ClockPhase::kRisingEdge,
       .bits_per_word = pw::spi::BitsPerWord(8),
       .bit_order = pw::spi::BitOrder::kLsbFirst,
   };

   auto initiator = pw::spi::MyInitator();
   auto mutex = pw::sync::VirtualMutex();
   auto selector = pw::spi::MyChipSelector();

   auto device = pw::spi::Device(
      pw::sync::Borrowable<Initiator>(initiator, mutex), kConfig, selector);

This example demonstrates the construction of a :doxylink:`Device
<pw::spi::Device>` from its object dependencies and configuration data; where
``MyDevice`` and ``MyChipSelector`` are concrete implementations of the
:doxylink:`Initiator <pw::spi::Initiator>` and :doxylink:`ChipSelector
<pw::spi::ChipSelector>` interfaces, respectively.

The use of :doxylink:`Borrowable <pw::sync::Borrowable>` in the interface
provides a mutual-exclusion wrapper for the injected :doxylink:`Initiator
<pw::spi::Initiator>`, ensuring that transactions cannot be interrupted or
corrupted by other concurrent workloads making use of the same SPI bus.

Once constructed, the ``device`` object can then be passed to functions used to
perform SPI transfers with a target responder.

Performing a transfer
=====================
.. code-block:: cpp

   pw::Result<SensorData> ReadSensorData(pw::spi::Device& device) {
     std::array<std::byte, 16> raw_sensor_data;
     constexpr std::array<std::byte, 2> kAccelReportCommand = {
         std::byte{0x13}, std::byte{0x37}};

     // This device supports full-duplex transfers
     PW_TRY(device.WriteRead(kAccelReportCommand, raw_sensor_data));
     return UnpackSensorData(raw_sensor_data);
   }

The ``ReadSensorData()`` function implements a driver function for a contrived
SPI accelerometer.  The function performs a full-duplex transfer with the
device to read its current data.

As this function relies on the ``device`` object that abstracts the details
of bus-access and chip-selection, the function is portable to any target
that implements its underlying interfaces.

Performing a multi-part transaction
===================================
.. code-block:: cpp

   pw::Result<SensorData> ReadSensorData(pw::spi::Device& device) {
     std::array<std::byte, 16> raw_sensor_data;
     constexpr std::array<std::byte, 2> kAccelReportCommand = {
         std::byte{0x13}, std::byte{0x37}};

     // Creation of the RAII `transaction` acquires exclusive access to the bus
     pw::spi::Device::Transaction transaction =
       device.StartTransaction(pw::spi::ChipSelectBehavior::kPerTransaction);

     // This device only supports half-duplex transfers
     PW_TRY(transaction.Write(kAccelReportCommand));
     PW_TRY(transaction.Read(raw_sensor_data))

     return UnpackSensorData(raw_sensor_data);

     // Destruction of RAII `transaction` object releases lock on the bus
   }

The code above is similar to the previous example, but makes use of the
``Transaction`` API in :doxylink:`Device <pw::spi::Device>` to perform separate,
half-duplex ``Write()`` and ``Read()`` transfers, as is required by the sensor
in this example.

The use of the RAII ``transaction`` object in this example guarantees that
no other thread can perform transfers on the same SPI bus
(:doxylink:`Initiator <pw::spi::Initiator>`) until it goes out-of-scope.

Responding to an initiator
==========================
.. code-block:: cpp

   MyResponder responder;
   responder.SetCompletionHandler([](ByteSpan rx_data, Status status) {
     // Handle incoming data from initiator.
     // ...
     // Prepare data to send back to initiator during next SPI transaction.
     responder.WriteReadAsync(tx_data, rx_data);
   });

   // Prepare data to send back to initiator during next SPI transaction.
   responder.WriteReadAsync(tx_data, rx_data)

Mocking transactions
====================
:doxylink:`MockInitiator <pw::spi::MockInitiator>` is a generic mocked backend
for ``Initiator`` that is specifically intended for use when developing drivers
for SPI devices. It's structured around a set of "transactions" where each
transaction contains a write, a read, and a status. A transaction list can then
be passed to the ``MockInitiator``, where each consecutive call to
read/write will iterate to the next transaction in the list. Example:

.. code-block:: cpp

   using pw::spi::MakeExpectedTransactionlist;
   using pw::spi::MockInitiator;
   using pw::spi::MockWriteTransaction;

   constexpr auto kExpectWrite1 = pw::bytes::Array<1, 2, 3, 4, 5>();
   constexpr auto kExpectWrite2 = pw::bytes::Array<3, 4, 5>();
   auto expected_transactions = MakeExpectedTransactionArray(
       {MockWriteTransaction(pw::OkStatus(), kExpectWrite1),
        MockWriteTransaction(pw::OkStatus(), kExpectWrite2)});
   MockInitiator spi_mock(expected_transactions);

   // Begin driver code
   ConstByteSpan write1 = kExpectWrite1;
   // write1 is ok as spi_mock expects {1, 2, 3, 4, 5} == {1, 2, 3, 4, 5}
   Status status = spi_mock.WriteRead(write1, ConstByteSpan());

   // Takes the first two bytes from the expected array to build a mismatching
   // span to write.
   ConstByteSpan write2 = pw::span(kExpectWrite2).first(2);
   // write2 fails as spi_mock expects {3, 4, 5} != {3, 4}
   status = spi_mock.WriteRead(write2, ConstByteSpan());
   // End driver code

   // Optionally check if the mocked transaction list has been exhausted.
   // Alternatively this is also called from MockInitiator::~MockInitiator().
   EXPECT_EQ(spi_mock.Finalize(), OkStatus());

-------------
API reference
-------------
Moved: :doxylink:`pw_spi`

.. toctree::
   :hidden:
   :maxdepth: 1

   backends
