.. _module-pw_spi:

======
pw_spi
======
Pigweed's SPI module provides a set of interfaces for communicating with SPI
peripherals attached to a target.

--------
Overview
--------
The ``pw_spi`` module provides a series of interfaces that facilitate the
development of SPI peripheral drivers that are abstracted from the target's
SPI hardware implementation.  The interface consists of three main classes:

- ``pw::spi::Initiator`` - Interface for configuring a SPI bus, and using it
  to transmit and receive data.
- ``pw::spi::ChipSelector`` - Interface for enabling/disabling a SPI
  peripheral attached to the bus.
- ``pw::spi::Device`` - primary HAL interface used to interact with a SPI
  peripheral.

``pw_spi`` relies on a target-specific implementations of
``pw::spi::Initiator`` and ``pw::spi::ChipSelector`` to be defined, and
injected into ``pw::spi::Device`` objects which are used to communicate with a
given peripheral attached to a target's SPI bus.

Example - Constructing a SPI Device:

.. code-block:: cpp

   constexpr pw::spi::Config kConfig = {
       .polarity = pw::spi::ClockPolarity::kActiveHigh,
       .phase = pw::spi::ClockPhase::kRisingEdge,
       .bits_per_word = pw::spi::BitsPerWord(8),
       .bit_order = pw::spi::BitOrder::kLsbFirst,
   };

   auto initiator = pw::spi::MyInitator();
   auto selector = pw::spi::MyChipSelector();
   auto borrowable_initiator = pw::sync::Borrowable<Initiator&>(initiator);

   auto device = pw::spi::Device(borrowable_initiator, kConfig, selector);

This example demonstrates the construction of a ``pw::spi::Device`` from its
object dependencies and configuration data; where ``MyDevice`` and
`MyChipSelector`` are concrete implementations of the ``pw::spi::Initiator``
and ``pw::spi::ChipSelector`` interfaces, respectively.

The use of ``pw::sync::Borrowable`` in the interface provides a
mutual-exclusion wrapper for the the injected ``pw::spi::Initiator``, ensuring
that transactions cannot be interrupted or corrupted by other concurrent
workloads making use of the same SPI bus.

Once constructed, the ``device`` object can then be passed to functions used to
perform SPI transfers with a target peripheral.

Example - Performing a Transfer:

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

Example - Performing a Multi-part Transaction:

.. code-block:: cpp

   pw::Result<SensorData> ReadSensorData(pw::spi::Device& device) {
     std::array<std::byte, 16> raw_sensor_data;
     constexpr std::array<std::byte, 2> kAccelReportCommand = {
         std::byte{0x13}, std::byte{0x37}};

     // Creation of the RAII `transaction` acquires exclusive access to the bus
     std::optional<pw::spi::Device::Transaction> transaction =
       device.StartTransaction(pw::spi::ChipSelectBehavior::kPerTransaction);
     if (!transaction.has_value()) {
       return pw::Status::Unknown();
     )

     // This device only supports half-duplex transfers
     PW_TRY(transaction->Write(kAccelReportCommand));
     PW_TRY(transaction->Read(raw_sensor_data))

     return UnpackSensorData(raw_sensor_data);

     // Destruction of RAII `transaction` object releases lock on the bus
   }

The code above is similar to the previous example, but makes use of the
``Transaction`` API in ``pw::spi::Device`` to perform separate, half-duplex
``Write()`` and ``Read()`` transfers, as is required by the sensor in this
examplre.

The use of the RAII ``transaction`` object in this example guarantees that
no other thread can perform transfers on the same SPI bus
(``pw::spi::Initiator``) until it goes out-of-scope.

------------------
pw::spi Interfaces
------------------
The SPI API consists of the following components:

- The ``pw::spi::Initiator`` interface, and its associated configuration data
  structs.
- The ``pw::spi::ChipSelector`` interface.
- The ``pw::spi::Device`` class.

pw::spi::Initiator
------------------
.. inclusive-language: disable

The common interface for configuring a SPI bus, and initiating transfers using
it.

A concrete implementation of this interface class *must* be defined in order
to use ``pw_spi`` with a specific target.

The ``spi::Initiator`` object configures the SPI bus to communicate with a
defined set of common bus parameters that include:

- clock polarity/phase
- bits-per-word (between 3-32 bits)
- bit ordering (LSB or MSB first)

These bus configuration parameters are aggregated in the ``pw::spi::Config``
structure, and passed to the ``pw::spi::Initiator`` via its ``Configure()``
method.

.. Note:

   Throughtout ``pw_spi``, the terms "controller" and "peripheral" are used to
   describe the two roles SPI devices can implement.  These terms correspond
   to the  "master" and "slave" roles described in legacy documentation
   related to the SPI protocol.

   ``pw_spi`` only supports SPI transfers where the target implements the
   "controller" role, and does not support the target acting in the
   "peripheral" role.

.. inclusive-language: enable

.. cpp:class:: pw::spi::Initiator

   .. cpp:function:: Status Configure(const Config& config)

      Configure the SPI bus to coummunicate using a specific set of properties,
      including the clock polarity, clock phase, bit-order, and bits-per-word.

      Returns OkStatus() on success, and implementation-specific values on
      failure conditions

   .. cpp:function:: Status WriteRead(ConstByteSpan write_buffer, ByteSpan read_buffer) = 0;

      Perform a synchronous read/write operation on the SPI bus.  Data from the
      `write_buffer` object is written to the bus, while the `read_buffer` is
      populated with incoming data on the bus.  The operation will ensure that
      all requested data is written-to and read-from the bus. In the event the
      read buffer is smaller than the write buffer (or zero-size), any
      additional input bytes are discarded. In the event the write buffer is
      smaller than the read buffer (or zero size), the output is padded with
      0-bits for the remainder of the transfer.

      Returns OkStatus() on success, and implementation-specific values on
      failure.

pw::spi::ChipSelector
---------------------
The ChipSelector class provides an abstract interface for controlling the
chip-select signal associated with a specific SPI peripheral.

This interface provides a ``SetActive()`` method, which activates/deactivates
the device based on the value of the `active` parameter.  The associated
``Activate()`` and ``Deactivate()`` methods are utility wrappers for
``SetActive(true)`` and ``SetActive(false)``, respectively.

A concrete implementation of this interface class must be provided in order to
use the SPI HAL to communicate with a peripheral.

.. Note::

   `Active` does not imply a specific logic-level; it is left to the
   implementor to correctly map logic-levels to the device's active/inactive
   states.

.. cpp:class:: pw::spi::ChipSelector

   .. cpp:function:: Status SetActive(bool active)

      SetActive sets the state of the chip-select signal to the value
      represented by the `active` parameter.  Passing a value of `true` will
      activate the chip-select signal, and `false` will deactive the
      chip-select signal.

      Returns OkStatus() on success, and implementation-specific values on
      failure.

   .. cpp:function:: Status Activate()

      Helper method to activate the chip-select signal

      Returns OkStatus() on success, and implementation-specific values on
      failure.

   .. cpp:function:: Status Deactivate()

      Helper method to deactivate the chip-select signal

      Returns OkStatus() on success, and implementation-specific values on
      failure.

pw::spi::Device
---------------
This is primary object used by a client to interact with a target SPI device.
It provides a wrapper for an injected ``pw::spi::Initator`` object, using
its methods to configure the bus and perform individual SPI transfers.  The
injected ``pw::spi::ChipSelector`` object is used internally to activate and
de-actviate the device on-demand from within the data transfer methods.

The ``Read()``/``Write()``/``WriteRead()`` methods provide support for
performing inidividual transfers:  ``Read()`` and ``Write()`` perform
half-duplex operations, where ``WriteRead()`` provides support for
full-duplex transfers.

The ``StartTransaction()`` method provides support for performing multi-part
transfers consisting of a series of ``Read()``/``Write()``/``WriteRead()``
calls, during which the caller is guaranteed exclusive access to the
underlying bus.  The ``Transaction`` objects returned from this method
implements the RAII layer providing exclusive access to the bus; exclusive
access locking is released when the ``Transaction`` object is destroyed/goes
out of scope.

Mutual-exclusion to the ``pw::spi::Initiator`` object is provided by the use of
the ``pw::sync::Borrowable`` object, where the ``pw::spi::Initiator`` object is
"borrowed" for the duration of a transaction.

.. cpp:class:: pw::spi::Device

   .. cpp:function:: Status Read(Bytespan read_buffer)

      Synchronously read data from the SPI peripheral until the provided
      `read_buffer` is full.
      This call will configure the bus and activate/deactive chip select
      for the transfer

      Note: This call will block in the event that other clients are currently
      performing transactions using the same SPI Initiator.

      Returns OkStatus() on success, and implementation-specific values on
      failure.

   .. cpp:function:: Status Write(ConstByteSpan write_buffer)

      Synchronously write the contents of `write_buffer` to the SPI peripheral.
      This call will configure the bus and activate/deactive chip select
      for the transfer

      Note: This call will block in the event that other clients are currently
      performing transactions using the same SPI Initiator.

      Returns OkStatus() on success, and implementation-specific values on
      failure.

   .. cpp:function:: Status WriteRead(ConstByteSpan write_buffer, ByteSpan read_buffer)

      Perform a synchronous read/write transfer with the SPI peripheral. Data
      from the `write_buffer` object is written to the bus, while the
      `read_buffer` is populated with incoming data on the bus.  In the event
      the read buffer is smaller than the write buffer (or zero-size), any
      additional input bytes are discarded. In the event the write buffer is
      smaller than the read buffer (or zero size), the output is padded with
      0-bits for the remainder of the transfer.
      This call will configure the bus and activate/deactive chip select
      for the transfer

      Note: This call will block in the event that other clients are currently
      performing transactions using the same SPI Initiator.

      Returns OkStatus() on success, and implementation-specific values on
      failure.

   .. cpp:function:: Transaction StartTransaction(ChipSelectBehavior behavior)

      Begin a transaction with the SPI device.  This creates an RAII
      `Transaction` object that ensures that only one entity can access the
      underlying SPI bus (Initiator) for the object's duration. The `behavior`
      parameter provides a means for a client to select how the chip-select
      signal will be applied on Read/Write/WriteRead calls taking place with
      the Transaction object. A value of `kPerWriteRead` will activate/deactive
      chip-select on each operation, while `kPerTransaction` will hold the
      chip-select active for the duration of the Transaction object.

.. cpp:class:: pw::spi::Device::Transaction

   .. cpp:function:: Status Read(Bytespan read_buffer)

      Synchronously read data from the SPI peripheral until the provided
      `read_buffer` is full.

      Returns OkStatus() on success, and implementation-specific values on
      failure.

   .. cpp:function:: Status Write(ConstByteSpan write_buffer)

      Synchronously write the contents of `write_buffer` to the SPI peripheral

      Returns OkStatus() on success, and implementation-specific values on
      failure.

   .. cpp:function:: Status WriteRead(ConstByteSpan write_buffer, ByteSpan read_buffer)

      Perform a synchronous read/write transfer on the SPI bus.  Data from the
      `write_buffer` object is written to the bus, while the `read_buffer` is
      populated with incoming data on the bus.  The operation will ensure that
      all requested data is written-to and read-from the bus. In the event the
      read buffer is smaller than the write buffer (or zero-size), any
      additional input bytes are discarded. In the event the write buffer is
      smaller than the read buffer (or zero size), the output is padded with
      0-bits for the remainder of the transfer.

      Returns OkStatus() on success, and implementation-specific values on
      failure.

