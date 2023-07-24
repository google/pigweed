.. _module-pw_stream_uart_linux:

====================
pw_stream_uart_linux
====================
``pw_stream_uart_linux`` implements the
:cpp:class:`pw::stream::NonSeekableReaderWriter` interface for reading from and
writing to a UART using Linux TTY interfaces.

.. note::
  This module will likely be superseded by a future ``pw_uart`` interface.

C++
===
.. doxygenclass:: pw::stream::UartStreamLinux
   :members:

Examples
========
A simple example illustrating writing to a UART:

.. code-block:: cpp

   constexpr const char* kUartPath = "/dev/ttyS0";
   constexpr uint32_t kBaudRate = 115200;

   pw::stream::UartStreamLinux stream;
   PW_TRY(stream.Open(kUartPath, kBaudRate));

   std::array<std::byte, 10> to_write = {};
   PW_TRY(stream.Write(to_write));

Caveats
=======
No interfaces are supplied for configuring data bits, stop bits, or parity.
Supported baud rates are limited but could be extended by modifying the ``Open``
method.