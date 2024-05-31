.. _module-pw_spi_mcuxpresso:

=================
pw_spi_mcuxpresso
=================
``pw_spi_mcuxpresso`` implements the :ref:`module-pw_spi` interfaces using the
NXP MCUXpresso SDK.

There are two initiator implementations corresponding to the SPI and FLEXIO_SPI
drivers in the SDK. SPI transfer can be configured to use a blocking
(by polling) method or non-blocking under the covers. The API is synchronous
regardless.

There is a responder implementation ``McuxpressoResponder`` which uses the SPI
and DMA drivers from the SDK.

-----
Setup
-----
Use of this module requires setting up the MCUXpresso SDK for use with Pigweed. Follow
the steps in :ref:`module-pw_build_mcuxpresso` to create a ``pw_source_set`` for an
MCUXpresso SDK. Include the GPIO and PINT driver components in this SDK definition.

This example shows what your SDK setup would look like if using an RT595 EVK.

.. code-block:: text

   import("$dir_pw_third_party/mcuxpresso/mcuxpresso.gni")

   pw_mcuxpresso_sdk("sample_project_sdk") {
     manifest = "$dir_pw_third_party/mcuxpresso/evkmimxrt595/EVK-MIMXRT595_manifest_v3_13.xml"
     include = [
       "component.serial_manager_uart.MIMXRT595S",
       "platform.drivers.flexio_spi.MIMXRT595S",
       "platform.drivers.flexspi.MIMXRT595S",
       "project_template.evkmimxrt595.MIMXRT595S",
       "utility.debug_console.MIMXRT595S",
     ]
   }

Next, specify the ``pw_third_party_mcuxpresso_SDK`` GN global variable to specify
the name of this source set. Edit your GN args with ``gn args out``.

.. code-block:: text

   pw_third_party_mcuxpresso_SDK = "//targets/mimxrt595_evk:sample_project_sdk"

Then, depend on this module in your BUILD.gn to use.

.. code-block:: text

   deps = [ dir_pw_spi_mcuxpresso ]

--------
Examples
--------
Example write using the FLEXIO_SPI initiator:

.. code-block:: text

   McuxpressoFlexIoInitiator spi(
      flexio_spi_config, CLOCK_GetFlexioClkFreq(), baud_rate_bps, blocking);
   spi.Configure(configuration);

   spi.WriteRead(source, destination);

Example use of SPI responder:

.. code-block:: cpp

   #include "pw_dma_mcuxpresso/dma.h"
   #include "pw_spi_mcuxpresso/responder.h"

   constinit pw::dma::McuxpressoDmaController dma(DMA0_BASE);

   pw::dma::McuxpressoDmaChannel tx_dma = dma.GetChannel(kTxDmaChannel);
   pw::dma::McuxpressoDmaChannel rx_dma = dma.GetChannel(kRxDmaChannel);

   pw::spi::McuxpressoResponder spi_responder(
      {
         // SPI mode 3 (CPOL = 1, CPHA = 1)
         .polarity = pw::spi::ClockPolarity::kActiveLow,  // CPOL = 1
         .phase = pw::spi::ClockPhase::kFallingEdge,      // CPHA = 1
         .bits_per_word = 8,
         .bit_order = pw::spi::BitOrder::kMsbFirst,
         .base_address = SPI14_BASE,
         .handle_cs = true,
      },
      tx_dma,
      rx_dma);

   pw::Status Init() {
     // Initialize the DMA controller
     PW_TRY(dma.Init());

     tx_dma.Init();
     tx_dma.SetPriority(kTxDmaChannelPriority);
     tx_dma.Enable();
     tx_dma.EnableInterrupts();

     rx_dma.Init();
     rx_dma.SetPriority(kRxDmaChannelPriority);
     rx_dma.Enable();
     tx_dma.EnableInterrupts();

     PW_TRY(spi_responder.Initialize());

     spi_responder.SetCompletionHandler([this](pw::ByteSpan rx_data, pw::Status status) {
      // Signal we got some data
     });

     // Start listen for read
     PW_TRY(spi_.WriteReadAsync(kTxData, rx_buf));
   }
