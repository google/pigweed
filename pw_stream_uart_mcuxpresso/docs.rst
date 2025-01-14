.. _module-pw_stream_uart_mcuxpresso:

=========================
pw_stream_uart_mcuxpresso
=========================
.. pigweed-module::
   :name: pw_stream_uart_mcuxpresso

``pw_stream_uart_mcuxpresso`` implements the ``pw_stream`` interface for reading
and writing to a UART using the NXP MCUXpresso SDK. ``UartStreamMcuxpresso``
version uses the CPU to read and write to the UART, while ``DmaUartMcuxpresso``
in :ref:`module-pw_uart_mcuxpresso` uses DMA transfers to read and write to the
UART minimizing the CPU utilization.

``InterruptSafeUartWriterMcuxpresso`` implements an interrupt safe
write-only stream to UART. Intended for use in fault handlers. It can be
constructed ``constinit`` for use in pre-static constructor environments as well.

.. note::

   This module is moving to :ref:`module-pw_uart_mcuxpresso`.

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

``UartStreamMcuxpresso`` example:

.. literalinclude:: stream_example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_stream_uart_mcuxpresso-UartStreamExample]
   :end-before: [pw_stream_uart_mcuxpresso-UartStreamExample]

``InterruptSafeUartWriterMcuxpresso`` example:

.. literalinclude:: interrupt_safe_writer_example.cc
   :language: cpp
   :linenos:
   :start-after: [pw_stream_uart_mcuxpresso-UartInterruptSafeWriterExample]
   :end-before: [pw_stream_uart_mcuxpresso-UartInterruptSafeWriterExample]
