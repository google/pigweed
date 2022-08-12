.. _module-pw_transfer:

===========
pw_transfer
===========
``pw_transfer`` is a reliable data transfer protocol which runs on top of
Pigweed RPC.

.. attention::

  ``pw_transfer`` is under construction and so is its documentation.

-----
Usage
-----

C++
===

Transfer thread
---------------
To run transfers as either a client or server (or both), a dedicated thread is
required. The transfer thread is used to process all transfer-related events
safely. The same transfer thread can be shared by a transfer client and service
running on the same system.

.. note::

   All user-defined transfer callbacks (i.e. the virtual interface of a
   ``TransferHandler`` or completion function in a transfer client) will be
   invoked from the transfer thread's context.

In order to operate, a transfer thread requires two buffers:

- The first is a *chunk buffer*. This is used to stage transfer packets received
  by the RPC system to be processed by the transfer thread. It must be large
  enough to store the largest possible chunk the system supports.

- The second is an *encode buffer*. This is used by the transfer thread to
  encode outgoing RPC packets. It is necessarily larger than the chunk buffer.
  Typically, this is sized to the system's maximum transmission unit at the
  transport layer.

A transfer thread is created by instantiating a ``pw::transfer::Thread``. This
class derives from ``pw::thread::ThreadCore``, allowing it to directly be used
when creating a system thread. Refer to :ref:`module-pw_thread-thread-creation`
for additional information.

**Example thread configuration**

.. code-block:: cpp

   #include "pw_transfer/transfer_thread.h"

   namespace {

   // The maximum number of concurrent transfers the thread should support as
   // either a client or a server. These can be set to 0 (if only using one or
   // the other).
   constexpr size_t kMaxConcurrentClientTransfers = 5;
   constexpr size_t kMaxConcurrentServerTransfers = 3;

   // The maximum payload size that can be transmitted by the system's
   // transport stack. This would typically be defined within some transport
   // header.
   constexpr size_t kMaxTransmissionUnit = 512;

   // The maximum amount of data that should be sent within a single transfer
   // packet. By necessity, this should be less than the max transmission unit.
   //
   // pw_transfer requires some additional per-packet overhead, so the actual
   // amount of data it sends may be lower than this.
   constexpr size_t kMaxTransferChunkSizeBytes = 480;

   // Buffers for storing and encoding chunks (see documentation above).
   std::array<std::byte, kMaxTransferChunkSizeBytes> chunk_buffer;
   std::array<std::byte, kMaxTransmissionUnit> encode_buffer;

   pw::transfer::Thread<kMaxConcurrentClientTransfers,
                        kMaxConcurrentServerTransfers>
       transfer_thread(chunk_buffer, encode_buffer);

   }  // namespace

   // pw::transfer::TransferThread is the generic, non-templated version of the
   // Thread class. A Thread can implicitly convert to a TransferThread.
   pw::transfer::TransferThread& GetSystemTransferThread() {
     return transfer_thread;
   }


Transfer server
---------------
``pw_transfer`` provides an RPC service for running transfers through an RPC
server.

To know how to read data from or write data to device, a ``TransferHandler``
interface is defined (``pw_transfer/public/pw_transfer/handler.h``). Transfer
handlers represent a transferable resource, wrapping a stream reader and/or
writer with initialization and completion code. Custom transfer handler
implementations should derive from ``ReadOnlyHandler``, ``WriteOnlyHandler``,
or ``ReadWriteHandler`` as appropriate and override Prepare and Finalize methods
if necessary.

A transfer handler should be implemented and instantiated for each unique
resource that can be transferred to or from a device. Each instantiated handler
must have a globally-unique integer ID used to identify the resource.

Handlers are registered with the transfer service. This may be done during
system initialization (for static resources), or dynamically at runtime to
support ephemeral transfer resources.

**Example transfer handler implementation**

.. code-block:: cpp

  #include "pw_stream/memory_stream.h"
  #include "pw_transfer/transfer.h"

  // A simple transfer handler which reads data from an in-memory buffer.
  class SimpleBufferReadHandler : public pw::transfer::ReadOnlyHandler {
   public:
    SimpleReadTransfer(uint32_t resource_id, pw::ConstByteSpan data)
        : ReadOnlyHandler(resource_id), reader_(data) {
      set_reader(reader_);
    }

   private:
    pw::stream::MemoryReader reader_;
  };

The transfer service is instantiated with a reference to the system's transfer
thread and registered with the system's RPC server.

**Example transfer service initialization**

.. code-block:: cpp

  #include "pw_transfer/transfer.h"

  namespace {

  // In a write transfer, the maximum number of bytes to receive at one time
  // (potentially across multiple chunks), unless specified otherwise by the
  // transfer handler's stream::Writer.
  constexpr size_t kDefaultMaxBytesToReceive = 1024;

  pw::transfer::TransferService transfer_service(
      GetSystemTransferThread(), kDefaultMaxBytesToReceive);

  // Instantiate a handler for the data to be transferred. The resource ID will
  // be used by the transfer client and server to identify the handler.
  constexpr uint32_t kMagicBufferResourceId = 1;
  char magic_buffer_to_transfer[256] = { /* ... */ };
  SimpleBufferReadHandler magic_buffer_handler(
      kMagicBufferResourceId, magic_buffer_to_transfer);

  }  // namespace

  void InitTransferService() {
    // Register the handler with the transfer service, then the transfer service
    // with an RPC server.
    transfer_service.RegisterHandler(magic_buffer_handler);
    GetSystemRpcServer().RegisterService(transfer_service);
  }

Transfer client
---------------
``pw_transfer`` provides a transfer client capable of running transfers through
an RPC client.

.. note::

   Currently, a transfer client is only capable of running transfers on a single
   RPC channel. This may be expanded in the future.

The transfer client provides the following two APIs for starting data transfers:

.. cpp:function:: pw::Status pw::transfer::Client::Read(uint32_t resource_id, pw::stream::Writer& output, CompletionFunc&& on_completion, pw::chrono::SystemClock::duration timeout = cfg::kDefaultChunkTimeout, pw::transfer::ProtocolVersion version = kDefaultProtocolVersion)

  Reads data from a transfer server to the specified ``pw::stream::Writer``.
  Invokes the provided callback function with the overall status of the
  transfer.

  Due to the asynchronous nature of transfer operations, this function will only
  return a non-OK status if it is called with bad arguments. Otherwise, it will
  return OK and errors will be reported through the completion callback.

.. cpp:function:: pw::Status pw::transfer::Client::Write(uint32_t resource_id, pw::stream::Reader& input, CompletionFunc&& on_completion, pw::chrono::SystemClock::duration timeout = cfg::kDefaultChunkTimeout, pw::transfer::ProtocolVersion version = kDefaultProtocolVersion)

  Writes data from a source ``pw::stream::Reader`` to a transfer server.
  Invokes the provided callback function with the overall status of the
  transfer.

  Due to the asynchronous nature of transfer operations, this function will only
  return a non-OK status if it is called with bad arguments. Otherwise, it will
  return OK and errors will be reported through the completion callback.

**Example client setup**

.. code-block:: cpp

   #include "pw_transfer/client.h"

   namespace {

   // RPC channel on which transfers should be run.
   constexpr uint32_t kChannelId = 42;

   pw::transfer::Client transfer_client(
       GetSystemRpcClient(), kChannelId, GetSystemTransferThread());

   }  // namespace

   Status ReadMagicBufferSync(pw::ByteSpan sink) {
     pw::stream::Writer writer(sink);

     struct {
       pw::sync::ThreadNotification notification;
       pw::Status status;
     } transfer_state;

     transfer_client.Read(
         kMagicBufferResourceId,
         writer,
         [&transfer_state](pw::Status status) {
           transfer_state.status = status;
           transfer_state.notification.release();
         });

     // Block until the transfer completes.
     transfer_state.notification.acquire();
     return transfer_state.status;
   }

Module Configuration Options
----------------------------
The following configurations can be adjusted via compile-time configuration of
this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. c:macro:: PW_TRANSFER_DEFAULT_MAX_RETRIES

  The default maximum number of times a transfer should retry sending a chunk
  when no response is received. This can later be configured per-transfer.

.. c:macro:: PW_TRANSFER_DEFAULT_TIMEOUT_MS

  The default amount of time, in milliseconds, to wait for a chunk to arrive
  before retrying. This can later be configured per-transfer.

.. c:macro:: PW_TRANSFER_DEFAULT_EXTEND_WINDOW_DIVISOR

  The fractional position within a window at which a receive transfer should
  extend its window size to minimize the amount of time the transmitter
  spends blocked.

  For example, a divisor of 2 will extend the window when half of the
  requested data has been received, a divisor of three will extend at a third
  of the window, and so on.

Python
======
.. automodule:: pw_transfer
  :members: ProgressStats, Manager, Error

**Example**

.. code-block:: python

  import pw_transfer

  # Initialize a Pigweed RPC client; see pw_rpc docs for more info.
  rpc_client = CustomRpcClient()
  rpcs = rpc_client.channel(1).rpcs

  transfer_service = rpcs.pw.transfer.Transfer
  transfer_manager = pw_transfer.Manager(transfer_service)

  try:
    # Read the transfer resource with ID 3 from the server.
    data = transfer_manager.read(3)
  except pw_transfer.Error as err:
    print('Failed to read:', err.status)

  try:
    # Send some data to the server. The transfer manager does not have to be
    # reinitialized.
    transfer_manager.write(2, b'hello, world')
  except pw_transfer.Error as err:
    print('Failed to write:', err.status)

Typescript
==========

Provides a simple interface for transferring bulk data over pw_rpc.

**Example**

.. code-block:: typescript

   import { pw_transfer } from 'pigweedjs';
   const { Manager } from pw_transfer;

   const client = new CustomRpcClient();
   service = client.channel()!.service('pw.transfer.Transfer')!;

   const manager = new Manager(service, DEFAULT_TIMEOUT_S);

   manager.read(3, (stats: ProgressStats) => {
     console.log(`Progress Update: ${stats}`);
   }).then((data: Uint8Array) => {
     console.log(`Completed read: ${data}`);
   }).catch(error => {
     console.log(`Failed to read: ${error.status}`);
   });

   manager.write(2, textEncoder.encode('hello world'))
     .catch(error => {
       console.log(`Failed to read: ${error.status}`);
     });

--------
Protocol
--------

Protocol buffer definition
==========================
.. literalinclude:: transfer.proto
  :language: protobuf
  :lines: 14-

Server to client transfer (read)
================================
.. image:: read.svg

Client to server transfer (write)
=================================
.. image:: write.svg

Errors
======

Protocol errors
---------------
At any point, either the client or server may terminate the transfer with a
status code. The transfer chunk with the status is the final chunk of the
transfer.

The following table describes the meaning of each status code when sent by the
sender or the receiver (see `Transfer roles`_).

.. cpp:namespace-push:: pw::stream

+-------------------------+-------------------------+-------------------------+
| Status                  | Sent by sender          | Sent by receiver        |
+=========================+=========================+=========================+
| ``OK``                  | (not sent)              | All data was received   |
|                         |                         | and handled             |
|                         |                         | successfully.           |
+-------------------------+-------------------------+-------------------------+
| ``ABORTED``             | The service aborted the transfer because the      |
|                         | client restarted it. This status is passed to the |
|                         | transfer handler, but not sent to the client      |
|                         | because it restarted the transfer.                |
+-------------------------+---------------------------------------------------+
| ``CANCELLED``           | The client cancelled the transfer.                |
+-------------------------+-------------------------+-------------------------+
| ``DATA_LOSS``           | Failed to read the data | Failed to write the     |
|                         | to send. The            | received data. The      |
|                         | :cpp:class:`Reader`     | :cpp:class:`Writer`     |
|                         | returned an error.      | returned an error.      |
+-------------------------+-------------------------+-------------------------+
| ``FAILED_PRECONDITION`` | Received chunk for transfer that is not active.   |
+-------------------------+-------------------------+-------------------------+
| ``INVALID_ARGUMENT``    | Received a malformed packet.                      |
+-------------------------+-------------------------+-------------------------+
| ``INTERNAL``            | An assumption of the protocol was violated.       |
|                         | Encountering ``INTERNAL`` indicates that there is |
|                         | a bug in the service or client implementation.    |
+-------------------------+-------------------------+-------------------------+
| ``PERMISSION_DENIED``   | The transfer does not support the requested       |
|                         | operation (either reading or writing).            |
+-------------------------+-------------------------+-------------------------+
| ``RESOURCE_EXHAUSTED``  | The receiver requested  | Storage is full.        |
|                         | zero bytes, indicating  |                         |
|                         | their storage is full,  |                         |
|                         | but there is still data |                         |
|                         | to send.                |                         |
+-------------------------+-------------------------+-------------------------+
| ``UNAVAILABLE``         | The service is busy with other transfers and      |
|                         | cannot begin a new transfer at this time.         |
+-------------------------+-------------------------+-------------------------+
| ``UNIMPLEMENTED``       | Out-of-order chunk was  | (not sent)              |
|                         | requested, but seeking  |                         |
|                         | is not supported.       |                         |
+-------------------------+-------------------------+-------------------------+

.. cpp:namespace-pop::

Client errors
-------------
``pw_transfer`` clients may immediately return certain errors if they cannot
start a transfer.

.. list-table::

  * - **Status**
    - **Reason**
  * - ``ALREADY_EXISTS``
    - A transfer with the requested ID is already pending on this client.
  * - ``DATA_LOSS``
    - Sending the initial transfer chunk failed.
  * - ``RESOURCE_EXHAUSTED``
    - The client has insufficient resources to start an additional transfer at
      this time.


Transfer roles
==============
Every transfer has two participants: the sender and the receiver. The sender
transmits data to the receiver. The receiver controls how the data is
transferred and sends the final status when the transfer is complete.

In read transfers, the client is the receiver and the service is the sender. In
write transfers, the client is the sender and the service is the receiver.

Sender flow
-----------
.. mermaid::

  graph TD
    start([Client initiates<br>transfer]) -->data_request
    data_request[Receive transfer<br>parameters]-->send_chunk

    send_chunk[Send chunk]-->sent_all

    sent_all{Sent final<br>chunk?} -->|yes|wait
    sent_all-->|no|sent_requested

    sent_requested{Sent all<br>pending?}-->|yes|data_request
    sent_requested-->|no|send_chunk

    wait[Wait for receiver]-->is_done

    is_done{Received<br>final chunk?}-->|yes|done
    is_done-->|no|data_request

    done([Transfer complete])

Receiver flow
-------------
.. mermaid::

  graph TD
    start([Client initiates<br>transfer]) -->request_bytes
    request_bytes[Set transfer<br>parameters]-->wait

    wait[Wait for chunk]-->received_chunk

    received_chunk{Received<br>chunk by<br>deadline?}-->|no|request_bytes
    received_chunk-->|yes|check_chunk

    check_chunk{Correct<br>offset?} -->|yes|process_chunk
    check_chunk --> |no|request_bytes

    process_chunk[Process chunk]-->final_chunk

    final_chunk{Final<br>chunk?}-->|yes|signal_completion
    final_chunk{Final<br>chunk?}-->|no|received_requested

    received_requested{Received all<br>pending?}-->|yes|request_bytes
    received_requested-->|no|wait

    signal_completion[Signal completion]-->done

    done([Transfer complete])


-----------------
Integration tests
-----------------
The ``pw_transfer`` module has a set of integration tests that verify the
correctness of implementations in different languages.
`Test source code <https://cs.pigweed.dev/pigweed/+/main:pw_transfer/integration_test/>`_.

To run the tests on your machine, run

.. code:: bash

  $ bazel run pw_transfer/integration_test:cross_language_integration_test

The integration tests permit injection of client/server/proxy binaries to use
when running the tests. This allows manual testing of older versions of
pw_transfer against newer versions.

.. code:: bash

  # Test a newer version of pw_transfer against an old C++ client that was
  # backed up to another directory.
  $ bazel run pw_transfer/integration_test:cross_language_integration_test -- \
      --cpp-client ../old_pw_transfer_version/cpp_client

CI/CQ integration
=================
`Current status of the test in CI <https://ci.chromium.org/p/pigweed/builders/ci/pigweed-integration-transfer>`_.

By default, these tests are not run in CQ (on presubmit) because they are too
slow. However, you can request that the tests be run in presubmit on your
change by adding to following line to the commit message footer:

.. code::

  Cq-Include-Trybots: luci.pigweed.try:pigweed-integration-transfer
