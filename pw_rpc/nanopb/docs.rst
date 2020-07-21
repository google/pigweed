.. default-domain:: cpp

.. highlight:: sh

.. _chapter-pw-rpc-nanopb:

------
nanopb
------
``pw_rpc`` can generate services which encode/decode RPC requests and responses
as nanopb message structs.

Usage
=====
To enable nanopb code generation, add ``nanopb_rpc`` as a generator to your
Pigweed target's ``pw_protobuf_GENERATORS`` list. Refer to
:ref:`chapter-pw-protobuf-compiler` for additional information.

.. code::

  # my_target/target_toolchains.gni

  defaults = {
    pw_protobuf_GENERATORS = [
      "pwpb",
      "nanopb_rpc",  # Enable RPC codegen
    ]
  }

Define a ``pw_proto_library`` containing the .proto file defining your service
(and optionally other related protos), then depend on the ``_nanopb_rpc``
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
    public_deps = [ ":chat_protos_nanopb_rpc" ]
  }

A C++ header file is generated for each input .proto file, with the ``.proto``
extension replaced by ``.rpc.pb.h``. For example, given the input file
``chat_protos/chat_service.proto``, the generated header file will be placed
at the include path ``"chat_protos/chat_service.rpc.pb.h"``.

Generated code API
==================
Take the following RPC service as an example.

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

.. cpp:function:: void ServerWriter::Finish(Status status = Status::OK)

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
