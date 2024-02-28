.. _module-pw_stream_uart_mcuxpresso:

=========================
pw_stream_uart_mcuxpresso
=========================
``pw_stream_uart_mcuxpresso`` implements the ``pw_stream`` interface for reading
and writing to a UART using the NXP MCUXpresso SDK. ``UartStreamMcuxpresso``
version uses the CPU to read and write to the UART, while ``UartStreamDmaMcuxpresso``
uses DMA transfers to read and write to the UART minimizing the CPU utilization.

.. note::
  This module will likely be superseded by a future ``pw_uart`` interface.

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

UartStreamMcuxpresso example:
.. code-block:: cpp

  constexpr uint32_t kFlexcomm = 0;
  constexpr uint32_t kBaudRate = 115200;
  std::array<std::byte, 20> kBuffer = {};

  auto stream = UartStreamMcuxpresso{USART0,
                                     kBaudRate,
                                     kUSART_ParityDisabled,
                                     kUSART_OneStopBit,
                                     kBuffer};

  PW_TRY(stream.Init(CLOCK_GetFlexcommClkFreq(kFlexcomm)));

  std::array<std::byte, 10> to_write = {};
  PW_TRY(stream.Write(to_write));


UartStreamDmaMcuxpresso example:
.. code-block:: cpp

  constexpr uint32_t kFlexcomm = 0;
  const auto kUartBase = USART0;
  constexpr uint32_t kBaudRate = 115200;
  std::array<std::byte, 65536> ring_buffer = {};
  constexpr uint32_t kUartRxDmaCh = 0;
  constexpr uint32_t kUartTxDmaCh = 1;

  const UartDmaStreamMcuxpresso::Config kConfig = {
      .usart_base = kUartBase,
      .baud_rate = kBaudRate,
      .parity = kUSART_ParityDisabled,
      .stop_bits = kUSART_OneStopBit,
      .dma_base = DMA0,
      .rx_dma_ch = kUartRxDmaCh,
      .tx_dma_ch = kUartTxDmaCh,
      .rx_input_mux_dmac_ch_request_en = kINPUTMUX_Flexcomm0RxToDmac0Ch0RequestEna,
      .tx_input_mux_dmac_ch_request_en = kINPUTMUX_Flexcomm0TxToDmac0Ch1RequestEna,
      .buffer = ByteSpan(ring_buffer)};

  auto stream = UartDmaStreamMcuxpresso{kConfig};

  PW_TRY(stream.Init(CLOCK_GetFlexcommClkFreq(kFlexcomm)));
