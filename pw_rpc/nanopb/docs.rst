.. _module-pw_rpc_nanopb:

------
nanopb
------
``pw_rpc`` can generate services which encode/decode RPC requests and responses
as nanopb message structs.

Usage
=====
To enable nanopb code generation, the build argument
``dir_pw_third_party_nanopb`` must be set to point to a local nanopb
installation.

Define a ``pw_proto_library`` containing the .proto file defining your service
(and optionally other related protos), then depend on the ``nanopb_rpc``
version of that library in the code implementing the service.

.. code::

  # chat/BUILD.gn

  import("$dir_pw_build/target_types.gni")
  import("$dir_pw_protobuf_compiler/proto.gni")

  pw_proto_library("chat_protos") {
    sources = [ "chat_protos/chat_service.proto" ]
  }

  # Library that implements the ChatService.
  pw_source_set("chat_service") {
    sources = [
      "chat_service.cc",
      "chat_service.h",
    ]
    public_deps = [ ":chat_protos.nanopb_rpc" ]
  }

A C++ header file is generated for each input .proto file, with the ``.proto``
extension replaced by ``.rpc.pb.h``. For example, given the input file
``chat_protos/chat_service.proto``, the generated header file will be placed
at the include path ``"chat_protos/chat_service.rpc.pb.h"``.

Generated code API
==================
All examples in this document use the following RPC service definition.

.. code:: protobuf

  // chat/chat_protos/chat_service.proto

  syntax = "proto3";

  service ChatService {
    // Returns information about a chatroom.
    rpc GetRoomInformation(RoomInfoRequest) returns (RoomInfoResponse) {}

    // Lists all of the users in a chatroom. The response is streamed as there
    // may be a large amount of users.
    rpc ListUsersInRoom(ListUsersRequest) returns (stream ListUsersResponse) {}

    // Uploads a file, in chunks, to a chatroom.
    rpc UploadFile(stream UploadFileRequest) returns (UploadFileResponse) {}

    // Sends messages to a chatroom while receiving messages from other users.
    rpc Chat(stream ChatMessage) returns (stream ChatMessage) {}
  }

Server-side
-----------
A C++ class is generated for each service in the .proto file. The class is
located within a special ``generated`` sub-namespace of the file's package.

The generated class is a base class which must be derived to implement the
service's methods. The base class is templated on the derived class.

.. code:: c++

  #include "chat_protos/chat_service.rpc.pb.h"

  class ChatService final : public generated::ChatService<ChatService> {
   public:
    // Implementations of the service's RPC methods; see below.
  };

Unary RPC
^^^^^^^^^
A unary RPC is implemented as a function which takes in the RPC's request struct
and populates a response struct to send back, with a status indicating whether
the request succeeded.

.. code:: c++

  pw::Status GetRoomInformation(pw::rpc::ServerContext& ctx,
                                const RoomInfoRequest& request,
                                RoomInfoResponse& response);

Server streaming RPC
^^^^^^^^^^^^^^^^^^^^
A server streaming RPC receives the client's request message alongside a
``ServerWriter``, used to stream back responses.

.. code:: c++

  void ListUsersInRoom(pw::rpc::ServerContext& ctx,
                       const ListUsersRequest& request,
                       pw::rpc::ServerWriter<ListUsersResponse>& writer);

The ``ServerWriter`` object is movable, and remains active until it is manually
closed or goes out of scope. The writer has a simple API to return responses:

.. cpp:function:: Status ServerWriter::Write(const T& response)

  Writes a single response message to the stream. The returned status indicates
  whether the write was successful.

.. cpp:function:: void ServerWriter::Finish(Status status = OkStatus())

  Closes the stream and sends back the RPC's overall status to the client.

Once a ``ServerWriter`` has been closed, all future ``Write`` calls will fail.

.. attention::

  Make sure to use ``std::move`` when passing the ``ServerWriter`` around to
  avoid accidentally closing it and ending the RPC.

Client streaming RPC
^^^^^^^^^^^^^^^^^^^^
.. attention::

  ``pw_rpc`` does not yet support client streaming RPCs.

Bidirectional streaming RPC
^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. attention::

  ``pw_rpc`` does not yet support bidirectional streaming RPCs.

Client-side
-----------
A corresponding client class is generated for every service defined in the proto
file. Like the service class, it is placed under the ``generated`` namespace.
The class is named after the service, with a ``Client`` suffix. For example, the
``ChatService`` would create a ``generated::ChatServiceClient``.

The client class contains static methods to call each of the service's methods.
It is not meant to be instantiated.

.. code-block:: c++

  static GetRoomInformationCall GetRoomInformation(
      Channel& channel,
      const RoomInfoRequest& request,
      ::pw::Function<void(Status, const RoomInfoResponse&)> on_response,
      ::pw::Function<void(Status)> on_rpc_error = nullptr);

The ``NanopbClientCall`` object returned by the RPC invocation stores the active
RPC's context. For more information on ``ClientCall`` objects, refer to the
:ref:`core RPC documentation <module-pw_rpc-making-calls>`. The type of the
returned object is complex, so it is aliased using the method name.

.. admonition:: Callback invocation

  RPC callbacks are invoked synchronously from ``Client::ProcessPacket``.

Method APIs
^^^^^^^^^^^
The first argument to each client call method is the channel through which to
send the RPC. Following that, the arguments depend on the method type.

Unary RPC
~~~~~~~~~
A unary RPC call takes the request struct and a callback to invoke when a
response is received. The callback receives the RPC's status and response
struct.

An optional second callback can be provided to handle internal errors.

.. code-block:: c++

  static GetRoomInformationCall GetRoomInformation(
      Channel& channel,
      const RoomInfoRequest& request,
      ::pw::Function<void(const RoomInfoResponse&, Status)> on_response,
      ::pw::Function<void(Status)> on_rpc_error = nullptr);

Server streaming RPC
~~~~~~~~~~~~~~~~~~~~
A server streaming RPC call takes the initial request struct and two callbacks.
The first is invoked on every stream response received, and the second is
invoked once the stream is complete with its overall status.

An optional third callback can be provided to handle internal errors.

.. code-block:: c++

  static ListUsersInRoomCall ListUsersInRoom(
      Channel& channel,
      const ListUsersRequest& request,
      ::pw::Function<void(const ListUsersResponse&)> on_response,
      ::pw::Function<void(Status)> on_stream_end,
      ::pw::Function<void(Status)> on_rpc_error = nullptr);

Example usage
^^^^^^^^^^^^^
The following example demonstrates how to call an RPC method using a nanopb
service client and receive the response.

.. code-block:: c++

  #include "chat_protos/chat_service.rpc.pb.h"

  namespace {
    MyChannelOutput output;
    pw::rpc::Channel channels[] = {pw::rpc::Channel::Create<0>(&output)};
    pw::rpc::Client client(channels);

    // Callback function for GetRoomInformation.
    void LogRoomInformation(const RoomInfoResponse& response, Status status);
  }

  void InvokeSomeRpcs() {
    // The RPC will remain active as long as `call` is alive.
    auto call = ChatServiceClient::GetRoomInformation(channels[0],
                                                      {.room = "pigweed"},
                                                      LogRoomInformation);

    // For simplicity, block here. An actual implementation would likely
    // std::move the call somewhere to keep it active while doing other work.
    while (call.active()) {
      Wait();
    }

    // Do other stuff now that we have the room information.
  }
