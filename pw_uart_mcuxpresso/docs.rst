.. _module-pw_uart_mcuxpresso:

==================
pw_uart_mcuxpresso
==================
.. pigweed-module::
   :name: pw_uart_mcuxpresso

``pw_uart_mcuxpresso`` implements the ``pw_uart`` interface for reading
and writing to a UART using the NXP MCUXpresso SDK.

There are two implementations available:

* `DmaUartMcuxpresso`: Implements `pw::Uart`.
* `DmaUartMcuxpressoNonBlocking`: Implements `pw::UartNonBlocking`.

Both implementations use DMA transfers to read andwrite to the UART,
minimizing CPU utilization.

.. note::
  For a simpler UART interface, see ``pw_stream_uart_mcuxpresso``.

Setup
=====
This module requires a little setup:

1. Use ``pw_build_mcuxpresso`` to create a ``pw_source_set`` for an
   MCUXpresso SDK.
2. Include the debug console component in this SDK definition.
3. Specify the ``pw_third_party_mcuxpresso_SDK`` GN global variable to specify
   the name of this source set.
4. Use a target that calls ``pw_sys_io_mcuxpresso_Init`` in
   ``pw_boot_PreMainInit`` or similar.

The name of the SDK source set must be set in the
"pw_third_party_mcuxpresso_SDK" GN arg

Usage
=====
---------------------
``DmaUartMcuxpresso``
---------------------

.. literalinclude:: dma_uart_example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_uart_mcuxpresso-DmaUartExample]
   :end-before: [pw_uart_mcuxpresso-DmaUartExample]


--------------------------------
``DmaUartMcuxpressoNonBlocking``
--------------------------------
.. literalinclude:: dma_uart_nonblocking_example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_uart_mcuxpresso-DmaUartNonBlockingBasicExample]
   :end-before: [pw_uart_mcuxpresso-DmaUartNonBlockingBasicExample]

The nonblocking UART can be converted to a blocking UART via an adapter:

.. literalinclude:: dma_uart_nonblocking_example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_uart_mcuxpresso-DmaUartNonBlockingAdapterExample]
   :end-before: [pw_uart_mcuxpresso-DmaUartNonBlockingAdapterExample]
