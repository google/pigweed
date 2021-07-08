.. _module-pw_transfer:

===========
pw_transfer
===========

.. attention::

  ``pw_transfer`` is under construction and so is its documentation.

-----
Usage
-----

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
