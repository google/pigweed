.. _module-pw_spi_linux:

.. cpp:namespace-push:: pw::spi

============
pw_spi_linux
============
.. pigweed-module::
   :name: pw_spi_linux

``pw_spi_linux`` implements the :ref:`module-pw_spi` interface using the Linux
userspace SPIDEV interface.

-------------
API reference
-------------
The following classes make up the public API:

``pw::spi::LinuxInitiator``
===========================
Implements the ``pw::spi::Initiator`` interface.

``pw::spi::LinuxChipSelector``
==============================
Implements the ``pw::spi::ChipSelector`` interface.

.. note::

   Chip selection is tied to the ``/dev/spidevB.C`` character device and is
   handled automatically by the kernel, so this class doesn't actually do
   anything.

------
Guides
------
Example code to use Linux SPI:

.. code-block:: cpp

   #include "pw_spi_linux/spi.h"
   #include "pw_status/try.h"

   pw::Status SpiExample() {
     constexpr uint32_t kSpiFreq = 24'000'000;

     constexpr pw::spi::Config kConfig = {
         .polarity = pw::spi::ClockPolarity::kActiveHigh,
         .phase = pw::spi::ClockPhase::kRisingEdge,
         .bits_per_word = pw::spi::BitsPerWord(8),
         .bit_order = pw::spi::BitOrder::kLsbFirst,
     };

     int fd = open("/dev/spidev0.0", O_RDWR);
     if (fd < 0) {
       return pw::Status::Internal();
     }

     pw::spi::LinuxInitiator initiator(fd, kSpiFreq);

     PW_TRY(initiator.Configure(kConfig));

     std::array tx_data = {std::byte(0xAA), std::byte(0x55)};
     std::array<std::byte, 8> rx_data;
     PW_TRY(initiator.WriteRead(tx_data, rx_data));
   }

