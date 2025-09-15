.. _module-pw_stream_uart_linux:

====================
pw_stream_uart_linux
====================
.. pigweed-module::
   :name: pw_stream_uart_linux

``pw_stream_uart_linux`` implements the
:doxylink:`pw::stream::NonSeekableReaderWriter` interface for reading from and
writing to a UART using Linux TTY interfaces.

.. note::
  This module will likely be superseded by a future ``pw_uart`` interface.

C++ API
=======
Moved: :doxylink:`pw_stream_uart_linux`

Examples
========
A simple example illustrating only changing baud-rate and writing to a UART:

.. code-block:: cpp

   constexpr const char* kUartPath = "/dev/ttyS0";
   constexpr pw::stream::UartStreamLinux::Config kConfig = {
       .baud_rate = 115200,
       // Flow control is unmodified on tty.
   };
   pw::stream::UartStreamLinux stream;
   PW_TRY(stream.Open(kUartPath, kConfig));

   std::array<std::byte, 10> to_write = {};
   PW_TRY(stream.Write(to_write));

A simple example illustrating enabling flow control and writing to a UART:

.. code-block:: cpp

   constexpr const char* kUartPath = "/dev/ttyS0";
   constexpr pw::stream::UartStreamLinux::Config kConfig = {
       .baud_rate = 115200,
       .flow_control = true,  // Enable hardware flow control.
   };
   pw::stream::UartStreamLinux stream;
   PW_TRY(stream.Open(kUartPath, kConfig));

   std::array<std::byte, 10> to_write = {};
   PW_TRY(stream.Write(to_write));

Caveats
=======
No interfaces are supplied for configuring data bits, stop bits, or parity.
These attributes are left as they are already configured on the TTY; only the
speed or flow control is modified.
