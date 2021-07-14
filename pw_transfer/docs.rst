.. _module-pw_transfer:

===========
pw_transfer
===========

.. attention::

  ``pw_transfer`` is under construction and so is its documentation.

-----
Usage
-----

C++
===
The transfer service is defined and registered with an RPC server like any other
RPC service.

To know how to read data from or write data to device, a ``TransferHandler``
interface is defined (``pw_transfer/public/pw_transfer/handler.h``). Transfer
handlers wrap a stream reader and/or writer with initialization and completion
code. Custom transfer handler implementations should derive from
``ReadOnlyHandler``, ``WriteOnlyHandler``, or ``ReadWriteHandler`` as
appropriate and override Prepare and Finalize methods if necessary.

A transfer handler should be implemented and instantiated for each unique data
transfer to or from a device. These handlers are then registered with the
transfer service using their transfer IDs.

**Example**

.. code-block:: cpp

  #include "pw_transfer/transfer.h"

  namespace {

  // Simple transfer handler which reads data from an in-memory buffer.
  class SimpleBufferReadHandler : public pw::transfer::ReadOnlyHandler {
   public:
    SimpleReadTransfer(uint32_t transfer_id, pw::ConstByteSpan data)
        : ReadOnlyHandler(transfer_id), reader_(data) {
      set_reader(reader_);
    }

   private:
    pw::stream::MemoryReader reader_;
  };

  // Instantiate a static transfer service.
  pw::transfer::TransferService transfer_service;

  // Instantiate a handler for the the data to be transferred.
  constexpr uint32_t kBufferTransferId = 1;
  char buffer_to_transfer[256] = { /* ... */ };
  SimpleBufferReadHandler buffer_handler(kBufferTransferId, buffer_to_transfer);

  }  // namespace

  void InitTransfer() {
    // Register the handler with the transfer service, then the transfer service
    // with an RPC server.
    transfer_service.RegisterHandler(buffer_handler);
    GetSystemRpcServer().RegisterService(transfer_service);
  }

Python
======
.. automodule:: pw_transfer.transfer
  :members: Manager, Error

**Example**

.. code-block:: python

  from pw_transfer import transfer

  # Initialize a Pigweed RPC client; see pw_rpc docs for more info.
  rpc_client = CustomRpcClient()
  rpcs = rpc_client.channel(1).rpcs

  transfer_service = rpcs.pw.transfer.Transfer
  transfer_manager = transfer.Manager(transfer_service)

  try:
    # Read transfer_id 3 from the server.
    data = transfer_manager.read(3)
  except transfer.Error as err:
    print('Failed to read:', err.status)

  try:
    # Send some data to the server. The transfer manager does not have to be
    # reinitialized.
    transfer_manager.write(2, b'hello, world')
  except transfer.Error as err:
    print('Failed to write:', err.status)

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
.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "set transfer parameters",
        leftnote = "transfer_id\noffset\npending_bytes\nmax_chunk_size\nchunk_delay"
    ];

    client <-- server [
        noactivate,
        label = "requested bytes\n(zero or more chunks)",
        rightnote = "transfer_id\noffset\ndata\n(remaining_bytes)"
    ];

    client --> server [
        noactivate,
        label = "update transfer parameters\n(as needed)",
        leftnote = "transfer_id\noffset\npending_bytes\n(max_chunk_size)\n(chunk_delay)"
    ];

    client <- server [
        noactivate,
        label = "final chunk",
        rightnote = "transfer_id\noffset\ndata\nremaining_bytes=0"
    ];

    client -> server [
        noactivate,
        label = "acknowledge completion",
        leftnote = "transfer_id\nstatus=OK"
    ];
  }

Client to server transfer (write)
=================================
.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "start",
        leftnote = "transfer_id"
    ];

    client <- server [
        noactivate,
        label = "set transfer parameters",
        rightnote = "transfer_id\noffset\npending_bytes\nmax_chunk_size\nchunk_delay"
    ];

    client --> server [
        noactivate,
        label = "requested bytes\n(zero or more chunks)",
        leftnote = "transfer_id\noffset\ndata\n(remaining_bytes)"
    ];

    client <-- server [
        noactivate,
        label = "update transfer parameters\n(as needed)",
        rightnote = "transfer_id\noffset\npending_bytes\n(max_chunk_size)\n(chunk_delay)"
    ];

    client -> server [
        noactivate,
        label = "final chunk",
        leftnote = "transfer_id\noffset\ndata\nremaining_bytes=0"
    ];

    client <- server [
        noactivate,
        label = "acknowledge completion",
        rightnote = "transfer_id\nstatus=OK"
    ];
  }

Errors
======
At any point, either the client or server may terminate the transfer by sending
an error chunk with the transfer ID and a non-OK status.

Transmitter flow
================
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
=============
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
