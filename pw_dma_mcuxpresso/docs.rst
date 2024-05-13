.. _module-pw_dma_mcuxpresso:

=================
pw_dma_mcuxpresso
=================
.. pigweed-module::
   :name: pw_dma_mcuxpresso

``pw_dma_mcuxpresso`` provides helper classes for working with an MCUXpresso DMA
controller. The API is specific to MCUXpresso and doesn't implement any generic
DMA API at this time.

-------------
API reference
-------------
The following classes make up the public API:

``McuxpressoDmaController``
===========================
Represents a DMA Controller. Supports ``constinit`` initialization.


``McuxpressoDmaChannel``
========================
Represents a single channel of a DMA controller.

NOTE: Because the SDK maintains a permanent reference to this class's
members, these objects must have static lifetime at the time Init() is
called and ever after. The destructor will intentionally crash.

------
Guides
------
Example code to use a DMA channel:

.. code-block:: cpp

   #include "pw_dma_mcuxpresso/dma.h"
   #include "pw_status/try.h"

   constinit pw::dma::McuxpressoDmaController dma(DMA0_BASE);

   // McuxpressoDmaChannel must have static lifetime
   pw::dma::McuxpressoDmaChannel tx_dma = dma.GetChannel(kTxDmaChannel);

   pw::Status Init() {
     // Initialize the DMA controller
     PW_TRY(dma.Init());

     tx_dma.Init();
     tx_dma.SetPriority(kTxDmaChannelPriority);
     tx_dma.Enable();
     tx_dma.EnableInterrupts();

     // Pass handle to driver that needs a dma_handle_t*.
     dma_handle_t* handle = tx_dma.handle();
   }
