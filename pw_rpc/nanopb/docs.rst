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

Client-side
-----------
A corresponding client class is generated for every service defined in the proto
file. Like the service class, it is placed under the ``generated`` namespace.
The class is named after the service, with a ``Client`` suffix. For example, the
``ChatService`` would create a ``generated::ChatServiceClient``.

The client class contains static methods to call each of the service's methods.
It is not meant to be instantiated. The signatures for the methods all follow
the same format, taking a channel through which to communicate, the initial
request struct, and a response handler.

.. code-block:: c++

  static NanopbClientCall<UnaryResponseHandler<RoomInfoResponse>>
  GetRoomInformation(Channel& channel,
                     const RoomInfoRequest& request,
                     UnaryResponseHandler<RoomInfoResponse> handler);

The ``NanopbClientCall`` object returned by the RPC invocation stores the active
RPC's context. For more information on ``ClientCall`` objects, refer to the
:ref:`core RPC documentation <module-pw_rpc-making-calls>`.

Response handlers
^^^^^^^^^^^^^^^^^
RPC responses are sent back to the caller through a response handler object.
These are classes with virtual callback functions implemented by the RPC caller
to handle RPC events.

There are two types of response handlers: unary and server-streaming, which are
used depending whether the method's responses are a stream or not.

Unary / client streaming RPC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
A ``UnaryResponseHandler`` is used by methods where the server returns a single
response. It contains a callback for the response, which is only called once.

.. code-block:: c++

  template <typename Response>
  class UnaryResponseHandler {
   public:
    virtual ~UnaryResponseHandler() = default;

    // Called when the response is received from the server with the method's
    // status and the deserialized response struct.
    virtual void ReceivedResponse(Status status, const Response& response) = 0;

    // Called when an error occurs internally in the RPC client or server.
    virtual void RpcError(Status) {}
  };

.. cpp:class:: template <typename Response> UnaryResponseHandler

  A handler for RPC methods which return a single response (i.e. unary and
  client streaming).

.. cpp:function:: virtual void UnaryResponseHandler::ReceivedResponse(Status status, const Response& response)

  Callback invoked when the response is recieved from the server. Guaranteed to
  only be called once.

.. cpp:function:: virtual void UnaryResponseHandler::RpcError(Status status)

  Callback invoked if an internal error occurs in the RPC system. Optional;
  defaults to a no-op.

**Example implementation**

.. code-block:: c++

  class RoomInfoHandler : public UnaryResponseHandler<RoomInfoResponse> {
   public:
    void ReceivedResponse(Status status,
                          const RoomInfoResponse& response) override {
      if (status.ok()) {
        response_ = response;
      }
    }

    constexpr RoomInfoResponse& response() { return response_; }

   private:
    RoomInfoResponse response_;
  };

Server streaming / bidirectional streaming RPC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
For methods which return a response stream, a ``ServerStreamingResponseHandler``
is used.

.. code:: c++

  class ServerStreamingResponseHandler {
   public:
    virtual ~ServerStreamingResponseHandler() = default;

    // Called on every response received from the server with the deserialized
    // response struct.
    virtual void ReceivedResponse(const Response& response) = 0;

    // Called when the server ends the stream with the overall RPC status.
    virtual void Complete(Status status) = 0;

    // Called when an error occurs internally in the RPC client or server.
    virtual void RpcError(Status) {}
  };

.. cpp:class:: template <typename Response> ServerStreamingResponseHandler

  A handler for RPC methods which return zero or more responses (i.e. server
  and bidirectional streaming).

.. cpp:function:: virtual void ServerStreamingResponseHandler::ReceivedResponse(const Response& response)

  Callback invoked whenever a response is received from the server.

.. cpp:function:: virtual void ServerStreamingResponseHandler::Complete(Status status)

  Callback invoked when the server ends the stream, with the overall status for
  the RPC.

.. cpp:function:: virtual void ServerStreamingResponseHandler::RpcError(Status status)

  Callback invoked if an internal error occurs in the RPC system. Optional;
  defaults to a no-op.

**Example implementation**

.. code-block:: c++

  class ChatHandler : public UnaryResponseHandler<ChatMessage> {
   public:
    void ReceivedResponse(const ChatMessage& response) override {
      gui_.RenderChatMessage(response);
    }

    void Complete(Status status) override {
      client_.Exit(status);
    }

   private:
    ChatGui& gui_;
    ChatClient& client_;
  };

Example usage
~~~~~~~~~~~~~
The following example demonstrates how to call an RPC method using a nanopb
service client and receive the response.

.. code-block:: c++

  #include "chat_protos/chat_service.rpc.pb.h"

  namespace {
    MyChannelOutput output;
    pw::rpc::Channel channels[] = {pw::rpc::Channel::Create<0>(&output)};
    pw::rpc::Client client(channels);
  }

  void InvokeSomeRpcs() {
    RoomInfoHandler handler;

    // The RPC will remain active as long as `call` is alive.
    auto call = ChatServiceClient::GetRoomInformation(channels[0],
                                                      {.room = "pigweed"},
                                                      handler);

    // For simplicity, block here. An actual implementation would likely
    // std::move the call somewhere to keep it active while doing other work.
    while (call.active()) {
      Wait();
    }

    DoStuff(handler.response());
  }
