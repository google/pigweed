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

----------------------
Command-Line Interface
----------------------
This module also provides a tool also named ``pw_spi_linux_cli`` which
provides a basic command-line interface to the library.

Usage:

.. code-block:: none

   Usage: pw_spi_linux_cli -D DEVICE -F FREQ [flags]

   Required flags:
     -D/--device   SPI device path (e.g. /dev/spidev0.0
     -F/--freq     SPI clock frequency in Hz (e.g. 24000000)

   Optional flags:
     -b/--bits     Bits per word, default: 8
     -h/--human    Human-readable output (default: binary, unless output to stdout tty)
     -i/--input    Input file, or - for stdin
                   If not given, no data is sent.
     -l/--lsb      LSB first (default: MSB first)
     -m/--mode     SPI mode (0-3), default: 0
     -o/--output   Output file (default: stdout)
     -r/--rx-count Number of bytes to receive (defaults to size of input)

Example:

.. code-block:: none

   $ echo -n "Hello world" | pw_spi_linux_cli --device=/dev/spidev1.0 \
     --freq=24000000 --mode=3 --input=- | hexdump -Cv

