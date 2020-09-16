.. default-domain:: cpp

.. highlight:: sh

.. _chapter-pw-rpc:

------
pw_rpc
------
The ``pw_rpc`` module provides a system for defining and invoking remote
procedure calls (RPCs) on a device.

.. attention::

  Under construction.

Services
========
A service is a logical grouping of RPCs defined within a .proto file. ``pw_rpc``
uses these .proto definitions to generate code for a base service, from which
user-defined RPCs are implemented.

``pw_rpc`` supports multiple protobuf libraries, and the generated code API
depends on which is used.

.. toctree::
  :maxdepth: 1

  nanopb/docs

Testing a pw_rpc integration
============================
After setting up a ``pw_rpc`` server in your project, you can test that it is
working as intended by registering the provided ``EchoService``, defined in
``pw_rpc_protos/echo.proto``, which echoes back a message that it receives.

.. literalinclude:: pw_rpc_protos/echo.proto
  :language: protobuf
  :lines: 14-

For example, in C++ with nanopb:

.. code:: c++

  #include "pw_rpc/server.h"

  // Include the apporpriate header for your protobuf library.
  #include "pw_rpc/echo_service_nanopb.h"

  constexpr pw::rpc::Channel kChannels[] = { /* ... */ };
  static pw::rpc::Server server(kChannels);

  static pw::rpc::EchoService echo_service;

  void Init() {
    server.RegisterService(&echo_service);
  }

Protocol description
====================
Pigweed RPC servers and clients communicate using ``pw_rpc`` packets. These
packets are used to send requests and responses, control streams, cancel ongoing
RPCs, and report errors.

Packet format
-------------
Pigweed RPC packets consist of a type and a set of fields. The packets are
encoded as protocol buffers. The full packet format is described in
``pw_rpc/pw_rpc_protos/packet.proto``.

.. literalinclude:: pw_rpc_protos/packet.proto
  :language: protobuf
  :lines: 14-

The packet type and RPC type determine which fields are present in a Pigweed RPC
packet. Each packet type is only sent by either the client or the server.
These tables describe the meaning of and fields included with each packet type.

Client-to-server packets
^^^^^^^^^^^^^^^^^^^^^^^^
+---------------------------+----------------------------------+
| packet type               | description                      |
+===========================+==================================+
| REQUEST                   | RPC request                      |
|                           |                                  |
|                           | .. code-block:: text             |
|                           |                                  |
|                           |   - channel_id                   |
|                           |   - service_id                   |
|                           |   - method_id                    |
|                           |   - payload                      |
|                           |     (unless first client stream) |
|                           |                                  |
+---------------------------+----------------------------------+
| CLIENT_STREAM_END         | Client stream finished           |
|                           |                                  |
|                           | .. code-block:: text             |
|                           |                                  |
|                           |   - channel_id                   |
|                           |   - service_id                   |
|                           |   - method_id                    |
|                           |                                  |
|                           |                                  |
+---------------------------+----------------------------------+
| CLIENT_ERROR              | Received unexpected packet       |
|                           |                                  |
|                           | .. code-block:: text             |
|                           |                                  |
|                           |   - channel_id                   |
|                           |   - service_id                   |
|                           |   - method_id                    |
|                           |   - status                       |
+---------------------------+----------------------------------+
| CANCEL_SERVER_STREAM      | Cancel a server stream           |
|                           |                                  |
|                           | .. code-block:: text             |
|                           |                                  |
|                           |   - channel_id                   |
|                           |   - service_id                   |
|                           |   - method_id                    |
|                           |                                  |
+---------------------------+----------------------------------+

**Errors**

The client sends ``CLIENT_ERROR`` packets to a server when it receives a packet
it did not request. If the RPC is a streaming RPC, the server should abort it.

The status code indicates the type of error. If the client does not distinguish
between the error types, it can send whichever status is most relevant. The
status code is logged, but all status codes result in the same action by the
server: aborting the RPC.

* ``NOT_FOUND`` -- Received a packet for a service method the client does not
  recognize.
* ``FAILED_PRECONDITION`` -- Received a packet for a service method that the
  client did not invoke.

Server-to-client packets
^^^^^^^^^^^^^^^^^^^^^^^^
+-------------------+--------------------------------+
| packet type       | description                    |
+===================+================================+
| RESPONSE          | RPC response                   |
|                   |                                |
|                   | .. code-block:: text           |
|                   |                                |
|                   |   - channel_id                 |
|                   |   - service_id                 |
|                   |   - method_id                  |
|                   |   - payload                    |
|                   |   - status                     |
|                   |     (unless in server stream)  |
+-------------------+--------------------------------+
| SERVER_STREAM_END | Server stream and RPC finished |
|                   |                                |
|                   | .. code-block:: text           |
|                   |                                |
|                   |   - channel_id                 |
|                   |   - service_id                 |
|                   |   - method_id                  |
|                   |   - status                     |
+-------------------+--------------------------------+
| SERVER_ERROR      | Received unexpected packet     |
|                   |                                |
|                   | .. code-block:: text           |
|                   |                                |
|                   |   - channel_id                 |
|                   |   - service_id (if relevant)   |
|                   |   - method_id (if relevant)    |
|                   |   - status                     |
+-------------------+--------------------------------+

**Errors**

The server sends ``SERVER_ERROR`` packets when it receives a packet it cannot
process. The client should abort any RPC for which it receives an error. The
status field indicates the type of error.

* ``NOT_FOUND`` -- The requested service or method does not exist.
* ``FAILED_PRECONDITION`` -- Attempted to cancel an RPC that is not pending.
* ``RESOURCE_EXHAUSTED`` -- The request came on a new channel, but a channel
  could not be allocated for it.
* ``INTERNAL`` -- The server was unable to respond to an RPC due to an
  unrecoverable internal error.

Inovking a service method
-------------------------
Calling an RPC requires a specific sequence of packets. This section describes
the protocol for calling service methods of each type: unary, server streaming,
client streaming, and bidirectional streaming.

Unary RPC
^^^^^^^^^
In a unary RPC, the client sends a single request and the server sends a single
response.

.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "request",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client <- server [
        label = "response",
        rightnote = "PacketType.RESPONSE\nchannel ID\nservice ID\nmethod ID\npayload\nstatus"
    ];
  }

Server streaming RPC
^^^^^^^^^^^^^^^^^^^^
In a server streaming RPC, the client sends a single request and the server
sends any number of responses followed by a ``SERVER_STREAM_END`` packet.

.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "request",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client <-- server [
        noactivate,
        label = "responses (zero or more)",
        rightnote = "PacketType.RESPONSE\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client <- server [
        label = "done",
        rightnote = "PacketType.SERVER_STREAM_END\nchannel ID\nservice ID\nmethod ID\nstatus"
    ];
  }

Server streaming RPCs may be cancelled by the client. The client sends a
``CANCEL_SERVER_STREAM`` packet to terminate the RPC.

.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "request",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client <-- server [
        noactivate,
        label = "responses (zero or more)",
        rightnote = "PacketType.RESPONSE\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client -> server [
        noactivate,
        label = "cancel",
        leftnote  = "PacketType.CANCEL_SERVER_STREAM\nchannel ID\nservice ID\nmethod ID"
    ];

    client <- server [
        label = "done",
        rightnote = "PacketType.SERVER_STREAM_END\nchannel ID\nservice ID\nmethod ID\nstatus"
    ];
  }

Client streaming RPC
^^^^^^^^^^^^^^^^^^^^
In a client streaming RPC, the client sends any number of RPC requests followed
by a ``CLIENT_STREAM_END`` packet. The server then sends a single response.

The first client-to-server RPC packet does not include a payload.

.. attention::

  ``pw_rpc`` does not yet support client streaming RPCs.

.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "start",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID"
    ];

    client --> server [
        noactivate,
        label = "requests (zero or more)",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client -> server [
        noactivate,
        label = "done",
        leftnote = "PacketType.CLIENT_STREAM_END\nchannel ID\nservice ID\nmethod ID"
    ];

    client <- server [
        label = "response",
        rightnote = "PacketType.RESPONSE\nchannel ID\nservice ID\nmethod ID\npayload\nstatus"
    ];
  }

The server may terminate a client streaming RPC at any time by sending its
response packet.

.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "start",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID"
    ];

    client --> server [
        noactivate,
        label = "requests (zero or more)",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client <- server [
        label = "response",
        rightnote = "PacketType.RESPONSE\nchannel ID\nservice ID\nmethod ID\npayload\nstatus"
    ];
  }

Bidirectional streaming RPC
^^^^^^^^^^^^^^^^^^^^^^^^^^^
In a bidirectional streaming RPC, the client sends any number of requests and
the server sends any number of responses. The client sends a
``CLIENT_STREAM_END`` packet when it has finished sending requests. The server
sends a ``SERVER_STREAM_END`` packet after it receives the client's
``CLIENT_STREAM_END`` and finished sending its responses.

The first client-to-server RPC packet does not include a payload.

.. attention::

  ``pw_rpc`` does not yet support bidirectional streaming RPCs.

.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "start",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID"
    ];

    client --> server [
        noactivate,
        label = "requests (zero or more)",
        leftnote = "PacketType.REQUEST\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    ... (messages in any order) ...

    client <-- server [
        noactivate,
        label = "responses (zero or more)",
        rightnote = "PacketType.RESPONSE\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client -> server [
        noactivate,
        label = "done",
        leftnote = "PacketType.CLIENT_STREAM_END\nchannel ID\nservice ID\nmethod ID"
    ];

    client <-- server [
        noactivate,
        label = "responses (zero or more)",
        rightnote = "PacketType.RPC\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client <- server [
        label = "done",
        rightnote = "PacketType.SERVER_STREAM_END\nchannel ID\nservice ID\nmethod ID\nstatus"
    ];
  }

The server may terminate the RPC at any time by sending a ``SERVER_STREAM_END``
packet with the status, even if the client has not sent its ``STREAM_END``. The
client may cancel the RPC at any time by sending a ``CANCEL_SERVER_STREAM``
packet.

.. seqdiag::
  :scale: 110

  seqdiag {
    default_note_color = aliceblue;

    client -> server [
        label = "start",
        leftnote = "PacketType.RPC\nchannel ID\nservice ID\nmethod ID"
    ];

    client --> server [
        noactivate,
        label = "requests (zero or more)",
        leftnote = "PacketType.RPC\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client <-- server [
        noactivate,
        label = "responses (zero or more)",
        rightnote = "PacketType.RPC\nchannel ID\nservice ID\nmethod ID\npayload"
    ];

    client -> server [
        noactivate,
        label = "cancel",
        leftnote = "PacketType.CANCEL_SERVER_STREAM\nchannel ID\nservice ID\nmethod ID"
    ];

    client <- server [
        label = "done",
        rightnote = "PacketType.STREAM_END\nchannel ID\nservice ID\nmethod ID\nstatus"
    ];
  }

RPC server
==========
Declare an instance of ``rpc::Server`` and register services with it.

.. admonition:: TODO

  Document the public interface

Size report
-----------
The following size report showcases the memory usage of the core RPC server. It
is configured with a single channel using a basic transport interface that
directly reads from and writes to ``pw_sys_io``. The transport has a 128-byte
packet buffer, which comprises the plurality of the example's RAM usage. This is
not a suitable transport for an actual product; a real implementation would have
additional overhead proportional to the complexity of the transport.

.. include:: server_size

RPC server implementation
-------------------------

The Method class
^^^^^^^^^^^^^^^^
The RPC Server depends on the ``pw::rpc::internal::Method`` class. ``Method``
serves as the bridge between the ``pw_rpc`` server library and the user-defined
RPC functions. ``Method`` takes an RPC packet, decodes it using a protobuf
library (if applicable), and calls the RPC function. Since ``Method`` interacts
directly with the protobuf library, it must be implemented separately for each
protobuf library.

``pw::rpc::internal::Method`` is not implemented as a facade with different
backends. Instead, there is a separate instance of the ``pw_rpc`` server library
for each ``Method`` implementation. There are a few reasons for this.

* ``Method`` is entirely internal to ``pw_rpc``. Users will never implement a
  custom backend. Exposing a facade would unnecessarily expose implementation
  details and make ``pw_rpc`` more difficult to use.
* There is no common interface between ``pw_rpc`` / ``Method`` implementations.
  It's not possible to swap between e.g. a Nanopb and a ``pw_protobuf`` RPC
  server because the interface for the user-implemented RPCs changes completely.
  This nullifies the primary benefit of facades.
* The different ``Method`` implementations can be built easily alongside one
  another in a cross-platform way. This makes testing simpler, since the tests
  build with any backend configuration. Users can select which ``Method``
  implementation to use simply by depending on the corresponding server library.

Packet flow
^^^^^^^^^^^

Requests
~~~~~~~~

.. blockdiag::

  blockdiag {
    packets [shape = beginpoint];

    group {
      label = "pw_rpc library";

      server [label = "Server"];
      service [label = "Service"];
      method [label = "internal::Method"];
    }

    stubs [label = "generated services", shape = ellipse];
    user [label = "user-defined RPCs", shape = roundedbox];

    packets -> server -> service -> method -> stubs -> user;
    packets -> server [folded];
    method -> stubs [folded];
  }

Responses
~~~~~~~~~

.. blockdiag::

  blockdiag {
    user -> stubs [folded];

    group {
      label = "pw_rpc library";

      server [label = "Server"];
      method [label = "internal::Method"];
      channel [label = "Channel"];
    }

    stubs [label = "generated services", shape = ellipse];
    user [label = "user-defined RPCs", shape = roundedbox];
    packets [shape = beginpoint];

    user -> stubs -> method [folded];
    method -> server -> channel;
    channel -> packets [folded];
  }
