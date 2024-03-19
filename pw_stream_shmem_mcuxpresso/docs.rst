.. _module-pw_stream_shmem_mcuxpresso:

==========================
pw_stream_shmem_mcuxpresso
==========================
``pw_stream_shmem_mcuxpresso`` implements the ``pw_stream`` interface for
reading and writing between two different processor cores via shared memory
using the NXP MCUXpresso SDK. It uses the messaging unit module (MU) to signal
data readiness between cores.

Setup
=====
This module requires a little setup:

1. Use ``pw_build_mcuxpresso`` to create a ``pw_source_set`` for an
   MCUXpresso SDK.
2. Include the debug console component in this SDK definition.
3. Specify the ``pw_third_party_mcuxpresso_SDK`` GN global variable to specify
   the name of this source set.

The name of the SDK source set must be set in the
"pw_third_party_mcuxpresso_SDK" GN arg

Usage
=====
``ShmemMcuxpressoStream`` blocks on both reads and writes, as only one
outstanding buffer can be transferred at a time in each direction. This means a
dedicated thread should be used for both reading and writing. A typical use case
for this class would be as the underlying transport for a pw_rpc network between
cores. Use with the ``pw::rpc::StreamRpcFrameSender`` and
``pw::rpc::StreamRpcDispatcher`` classes.

Interrupt handlers and shared buffers on both cores must be setup before using
this stream. The shared buffer must be mapped as uncacheable on both sides.

As an example on the RT595, we connect the M33 core to the FusionF1 DSP. On the
FusionF1 side, the MU interrupt must be explicitly routed.

Initialization for the M33 Core:

.. code-block:: cpp

   // `kSharedBuffer` is a pointer to memory that is shared between the M33 and
   // F1 cores, and it is at least `2 * kSharedBufferSize` in size.
   ByteSpan read_buffer = ByteSpan{kSharedBuffer, kSharedBufferSize};
   ByteSpan write_buffer = ByteSpan{kSharedBuffer + kSharedBufferSize, kSharedBufferSize};
   ShmemMcuxpressoStream stream{MUA, read_buffer, write_buffer};

   PW_EXTERN_C void MU_A_DriverIRQHandler() {
     stream.HandleInterrupt();
   }

   void Init() {
     return stream.Enable();
   }

Initialization for the FusionF1 Core:

.. code-block:: cpp

   ByteSpan write_buffer = ByteSpan{kSharedBuffer, kSharedBufferSize};
   ByteSpan read_buffer = ByteSpan{kSharedBuffer + kSharedBufferSize, kSharedBufferSize};
   ShmemMcuxpressoStream stream{MUB, read_buffer, write_buffer};

   PW_EXTERN_C void MU_B_IrqHandler(void*) {
     stream.HandleInterrupt();
   }

   void Init() {
    // Enables the clock for the Input Mux
    INPUTMUX_Init(INPUTMUX);
    // MUB interrupt signal is selected for DSP interrupt input 1
    INPUTMUX_AttachSignal(INPUTMUX, 1U, kINPUTMUX_MuBToDspInterrupt);
    // Disables the clock for the Input Mux to save power
    INPUTMUX_Deinit(INPUTMUX);

    xt_set_interrupt_handler(kMuBIrqNum, MU_B_IrqHandler, NULL);
    xt_interrupt_enable(kMuBIrqNum);
    stream.Enable();
   }

Read/Write example where each core has threads for reading and writing.

Core 0:

.. code-block:: cpp

   constexpr std::byte kCore0Value = std::byte{0xab};
   constexpr std::byte kCore1Value = std::byte{0xcd};

   void ReadThread() {
     while(true) {
       std::array<std::byte, 1> read = {};
       auto status = stream.Read(read);
       if (!status.ok() || status.size() != 1 || read[0] != kCore1Value) {
         PW_LOG_WARN("Incorrect value read from core1");
       }
     }
   }


   void WriteThread() {
     std::array<std::byte, 1> write = {kCore0Value};
     while(true) {
       stream.Write(write);
     }
   }

Core 1:

.. code-block:: cpp

   void ReadThread() {
    while(true) {
      std::array<std::byte, 1> read = {};
      auto status = stream.Read(read);
      if (!status.ok() || status.size() != 1 || read[0] != kCore0Value) {
        PW_LOG_WARN("Incorrect value read from core0");
      }
    }

  }

  void WriteThread() {
    std::array<std::byte, 1> write = {kCore1Value};
    while(true) {
      stream.Write(write);
    }
  }
