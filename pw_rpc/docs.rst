.. _module-pw_rpc:

------
pw_rpc
------
The ``pw_rpc`` module provides a system for defining and invoking remote
procedure calls (RPCs) on a device.

.. admonition:: Try it out!

  For a quick intro to ``pw_rpc``, see the
  :ref:`module-pw_hdlc_lite-rpc-example` in the :ref:`module-pw_hdlc_lite`
  module.

.. attention::

  This documentation is under construction.

Creating an RPC
===============

1. RPC service declaration
--------------------------
Pigweed RPCs are declared in a protocol buffer service definition.

* `Protocol Buffer service documentation
  <https://developers.google.com/protocol-buffers/docs/proto3#services>`_
* `gRPC service definition documentation
  <https://grpc.io/docs/what-is-grpc/core-concepts/#service-definition>`_

.. code-block:: protobuf

  syntax = "proto3";

  package foo.bar;

  message Request {}

  message Response {
    int32 number = 1;
  }

  service TheService {
    rpc MethodOne(Request) returns (Response) {}
    rpc MethodTwo(Request) returns (stream Response) {}
  }

This protocol buffer is declared in a ``BUILD.gn`` file as follows:

.. code-block:: python

  import("//build_overrides/pigweed.gni")
  import("$dir_pw_protobuf_compiler/proto.gni")

  pw_proto_library("the_service_proto") {
    sources = [ "foo_bar/the_service.proto" ]
  }

2. RPC code generation
----------------------
``pw_rpc`` generates a C++ header file for each ``.proto`` file. This header is
generated in the build output directory. Its exact location varies by build
system and toolchain, but the C++ include path always matches the sources
declaration in the ``pw_proto_library``. The ``.proto`` extension is replaced
with an extension corresponding to the protobuf library in use.

================== =============== =============== =============
Protobuf libraries Build subtarget Protobuf header pw_rpc header
================== =============== =============== =============
Raw only           .raw_rpc        (none)          .raw_rpc.pb.h
Nanopb or raw      .nanopb_rpc     .pb.h           .rpc.pb.h
pw_protobuf or raw .pwpb_rpc       .pwpb.h         .rpc.pwpb.h
================== =============== =============== =============

For example, the generated RPC header for ``"foo_bar/the_service.proto"`` is
``"foo_bar/the_service.rpc.pb.h"`` for Nanopb or
``"foo_bar/the_service.raw_rpc.pb.h"`` for raw RPCs.

The generated header defines a base class for each RPC service declared in the
``.proto`` file. A service named ``TheService`` in package ``foo.bar`` would
generate the following base class:

.. cpp:class:: template <typename Implementation> foo::bar::generated::TheService

3. RPC service definition
-------------------------
The serivce class is implemented by inheriting from the generated RPC service
base class and defining a method for each RPC. The methods must match the name
and function signature for one of the supported protobuf implementations.
Services may mix and match protobuf implementations within one service.

.. tip::

  The generated code includes RPC service implementation stubs. You can
  reference or copy and paste these to get started with implementing a service.
  These stub classes are generated at the bottom of the pw_rpc proto header.

A Nanopb implementation of this service would be as follows:

.. code-block:: cpp

  #include "foo_bar/the_service.rpc.pb.h"

  namespace foo::bar {

  class TheService : public generated::TheService<TheService> {
   public:
    pw::Status MethodOne(ServerContext& ctx,
                         const foo_bar_Request& request,
                         foo_bar_Response& response) {
      // implementation
      return pw::Status::Ok();
    }

    void MethodTwo(ServerContext& ctx,
                   const foo_bar_Request& request,
                   ServerWriter<foo_bar_Response>& response) {
      // implementation
      response.Write(foo_bar_Response{.number = 123});
    }
  };

  }  // namespace foo::bar

The Nanopb implementation would be declared in a ``BUILD.gn``:

.. code-block:: python

  import("//build_overrides/pigweed.gni")

  import("$dir_pw_build/target_types.gni")

  pw_source_set("the_service") {
    public_configs = [ ":public" ]
    public = [ "public/foo_bar/service.h" ]
    public_deps = [ ":the_service_proto.nanopb_rpc" ]
  }

.. attention::

  pw_rpc's generated classes will support using ``pw_protobuf`` or raw buffers
  (no protobuf library) in the future.

4. Register the service with a server
-------------------------------------
This example code sets up an RPC server with an :ref:`HDLC<module-pw_hdlc_lite>`
channel output and the example service.

.. code-block:: cpp

  // Set up the output channel for the pw_rpc server to use. This configures the
  // pw_rpc server to use HDLC over UART; projects not using UART and HDLC must
  // adapt this as necessary.
  pw::stream::SysIoWriter writer;
  pw::rpc::RpcChannelOutput<kMaxTransmissionUnit> hdlc_channel_output(
      writer, pw::hdlc_lite::kDefaultRpcAddress, "HDLC output");

  pw::rpc::Channel channels[] = {
      pw::rpc::Channel::Create<1>(&hdlc_channel_output)};

  // Declare the pw_rpc server with the HDLC channel.
  pw::rpc::Server server(channels);

  pw::rpc::TheService the_service;

  void RegisterServices() {
    // Register the foo.bar.TheService example service.
    server.Register(the_service);

    // Register other services
  }

  int main() {
    // Set up the server.
    RegisterServices();

    // Declare a buffer for decoding incoming HDLC frames.
    std::array<std::byte, kMaxTransmissionUnit> input_buffer;

    PW_LOG_INFO("Starting pw_rpc server");
    pw::hdlc_lite::ReadAndProcessPackets(
        server, hdlc_channel_output, input_buffer);
  }

Services
========
A service is a logical grouping of RPCs defined within a .proto file. ``pw_rpc``
uses these .proto definitions to generate code for a base service, from which
user-defined RPCs are implemented.

``pw_rpc`` supports multiple protobuf libraries, and the generated code API
depends on which is used.

.. _module-pw_rpc-protobuf-library-apis:

Protobuf library APIs
=====================

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
RPC functions. Each supported protobuf implementation extends ``Method`` to
implement its request and response proto handling. The ``pw_rpc`` server
calls into the ``Method`` implementation through the base class's ``Invoke``
function.

``Method`` implementations store metadata about each method, including a
function pointer to the user-defined method implementation. They also provide
``static constexpr`` functions for creating each type of method. ``Method``
implementations must satisfy the ``MethodImplTester`` test class in
``pw_rpc_private/method_impl_tester.h``.

See ``pw_rpc/internal/method.h`` for more details about ``Method``.

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

RPC client
==========
The RPC client is used to send requests to a server and manages the contexts of
ongoing RPCs.

Setting up a client
-------------------
The ``pw::rpc::Client`` class is instantiated with a list of channels that it
uses to communicate. These channels can be shared with a server, but multiple
clients cannot use the same channels.

To send incoming RPC packets from the transport layer to be processed by a
client, the client's ``ProcessPacket`` function is called with the packet data.

.. code:: c++

  #include "pw_rpc/client.h"

  namespace {

  pw::rpc::Channel my_channels[] = {
      pw::rpc::Channel::Create<1>(&my_channel_output)};
  pw::rpc::Client my_client(my_channels);

  }  // namespace

  // Called when the transport layer receives an RPC packet.
  void ProcessRpcPacket(ConstByteSpan packet) {
    my_client.ProcessPacket(packet);
  }

.. _module-pw_rpc-making-calls:

Making RPC calls
----------------
RPC calls are not made directly through the client, but using one of its
registered channels instead. A service client class is generated from a .proto
file for each selected protobuf library, which is then used to send RPC requests
through a given channel. The API for this depends on the protobuf library;
please refer to the
:ref:`appropriate documentation<module-pw_rpc-protobuf-library-apis>`. Multiple
service client implementations can exist simulatenously and share the same
``Client`` class.

When a call is made, a ``pw::rpc::ClientCall`` object is returned to the caller.
This object tracks the ongoing RPC call, and can be used to manage it. An RPC
call is only active as long as its ``ClientCall`` object is alive.

.. tip::
  Use ``std::move`` when passing around ``ClientCall`` objects to keep RPCs
  alive.

Client implementation details
-----------------------------

The ClientCall class
^^^^^^^^^^^^^^^^^^^^
``ClientCall`` stores the context of an active RPC, and serves as the user's
interface to the RPC client. The core RPC library provides a base ``ClientCall``
class with common functionality, which is then extended for RPC client
implementations tied to different protobuf libraries to provide convenient
interfaces for working with RPCs.

The RPC server stores a list of all of active ``ClientCall`` objects. When an
incoming packet is recieved, it dispatches to one of its active calls, which
then decodes the payload and presents it to the user.
