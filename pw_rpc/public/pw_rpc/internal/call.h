// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <cstddef>
#include <span>
#include <utility>

#include "pw_containers/intrusive_list.h"
#include "pw_function/function.h"
#include "pw_rpc/internal/call_context.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/config.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/method_type.h"
#include "pw_rpc/service.h"
#include "pw_status/status.h"

namespace pw::rpc::internal {

class Endpoint;
class Packet;

// Internal RPC Call class. The Call is used to respond to any type of RPC.
// Public classes like ServerWriters inherit from it with private inheritance
// and provide a public API for their use case. The Call's public API is used by
// the Server and Client classes.
//
// Private inheritance is used in place of composition or more complex
// inheritance hierarchy so that these objects all inherit from a common
// IntrusiveList::Item object. Private inheritance also gives the derived classs
// full control over their interfaces.
class Call : public IntrusiveList<Call>::Item {
 public:
  Call(const Call&) = delete;

  // Move support is provided to derived classes through the MoveFrom function.
  Call(Call&&) = delete;

  Call& operator=(const Call&) = delete;
  Call& operator=(Call&&) = delete;

  // True if the Call is active and ready to send responses.
  [[nodiscard]] bool active() const { return rpc_state_ == kActive; }

  // DEPRECATED: open() was renamed to active() because it is clearer and does
  //     not conflict with Open() in ReaderWriter classes.
  // TODO(pwbug/472): Remove the open() method.
  /* [[deprecated("Renamed to active()")]] */ bool open() const {
    return active();
  }

  uint32_t id() const { return id_; }

  uint32_t channel_id() const {
    return channel_ == nullptr ? Channel::kUnassignedChannelId : channel().id();
  }
  uint32_t service_id() const { return service_id_; }
  uint32_t method_id() const { return method_id_; }

  // Closes the Call and sends a RESPONSE packet, if it is active. Returns the
  // status from sending the packet, or FAILED_PRECONDITION if the Call is not
  // active.
  Status CloseAndSendResponse(ConstByteSpan response, Status status) {
    return CloseAndSendFinalPacket(PacketType::RESPONSE, response, status);
  }

  Status CloseAndSendResponse(Status status) {
    return CloseAndSendResponse({}, status);
  }

  Status CloseAndSendServerError(Status error) {
    return CloseAndSendFinalPacket(PacketType::SERVER_ERROR, {}, error);
  }

  Status CloseAndSendClientError(Status error) {
    return CloseAndSendFinalPacket(PacketType::CLIENT_ERROR, {}, error);
  }

  // Sends a payload in either a server or client stream packet.
  Status Write(ConstByteSpan payload);

  // Whenever a payload arrives (in a server/client stream or in a response),
  // call the on_next_ callback.
  void HandlePayload(ConstByteSpan message) const {
    if (on_next_) {
      on_next_(message);
    }
  }

  void HandleError(Status status) {
    Close();
    on_error(status);
  }

  bool has_client_stream() const { return HasClientStream(type_); }
  bool has_server_stream() const { return HasServerStream(type_); }

  bool client_stream_open() const {
    return client_stream_state_ == kClientStreamActive;
  }

  // Acquires a buffer into which to write a payload. The Call MUST be active
  // when this is called!
  ByteSpan AcquirePayloadBuffer();

  // Releases the buffer without sending a packet.
  void ReleasePayloadBuffer();

  // Keep this public so the Nanopb implementation can set it from a helper
  // function.
  void set_on_next(Function<void(ConstByteSpan)> on_next) {
    on_next_ = std::move(on_next);
  }

 protected:
  // Creates an inactive Call.
  constexpr Call()
      : endpoint_{},
        channel_{},
        id_{},
        service_id_{},
        method_id_{},
        rpc_state_{},
        type_{},
        call_type_{},
        client_stream_state_{} {}

  // Creates an active server-side Call.
  Call(const CallContext& context, MethodType type)
      : Call(context.server(),
             context.call_id(),
             context.channel().id(),
             context.service().id(),
             context.method().id(),
             type,
             kServerCall) {}

  // Creates an active client-side Call.
  Call(Endpoint& client,
       uint32_t channel_id,
       uint32_t service_id,
       uint32_t method_id,
       MethodType type);

  void MoveFrom(Call& other);

  Endpoint& endpoint() const { return *endpoint_; }
  Channel& channel() const { return *channel_; }

  void set_on_error(Function<void(Status)> on_error) {
    on_error_ = std::move(on_error);
  }

  // Calls the on_error callback without closing the RPC. This is used when the
  // call has already completed.
  void on_error(Status error) {
    if (on_error_) {
      on_error_(error);
    }
  }

  void MarkClientStreamCompleted() {
    client_stream_state_ = kClientStreamInactive;
  }

  constexpr const Channel::OutputBuffer& buffer() const { return response_; }

  // Sends a payload with the specified type. The payload may either be in an
  // previously acquired buffer or in a standalone buffer.
  //
  // Returns FAILED_PRECONDITION if the call is not active().
  Status SendPacket(PacketType type,
                    ConstByteSpan payload,
                    Status status = OkStatus());

  // Removes the RPC from the server & marks as closed. The responder must be
  // active when this is called.
  void Close();

  // Cancels an RPC. For client calls only.
  Status Cancel() {
    return CloseAndSendFinalPacket(PacketType::CANCEL, {}, OkStatus());
  }

 private:
  enum CallType : bool { kServerCall, kClientCall };

  // Common constructor for server & client calls.
  Call(Endpoint& endpoint,
       uint32_t id,
       uint32_t channel_id,
       uint32_t service_id,
       uint32_t method_id,
       MethodType type,
       CallType call_type);

  Packet MakePacket(PacketType type,
                    ConstByteSpan payload,
                    Status status = OkStatus()) const {
    return Packet(
        type, channel_id(), service_id(), method_id(), id_, payload, status);
  }

  Status CloseAndSendFinalPacket(PacketType type,
                                 ConstByteSpan response,
                                 Status status);

  internal::Endpoint* endpoint_;
  internal::Channel* channel_;
  uint32_t id_;
  uint32_t service_id_;
  uint32_t method_id_;

  Channel::OutputBuffer response_;

  // Called when the RPC is terminated due to an error.
  Function<void(Status error)> on_error_;

  // Called when a request is received. Only used for RPCs with client streams.
  // The raw payload buffer is passed to the callback.
  Function<void(ConstByteSpan payload)> on_next_;

  enum : bool { kInactive, kActive } rpc_state_;
  MethodType type_;
  CallType call_type_;
  enum : bool {
    kClientStreamInactive,
    kClientStreamActive,
  } client_stream_state_;
};

// A Call object, as used by an RPC server.
class ServerCall : public Call {
 public:
  void EndClientStream() {
    MarkClientStreamCompleted();

#if PW_RPC_CLIENT_STREAM_END_CALLBACK
    if (on_client_stream_end_) {
      on_client_stream_end_();
    }
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK
  }

 protected:
  ServerCall(ServerCall&& other) : Call() { *this = std::move(other); }

  ~ServerCall() {
    // Any errors are logged in Channel::Send.
    CloseAndSendResponse(OkStatus()).IgnoreError();
  }

  ServerCall& operator=(ServerCall&& other);

  constexpr ServerCall() = default;

  ServerCall(const CallContext& context, MethodType type)
      : Call(context, type) {}

  // set_on_client_stream_end is templated so that it can be conditionally
  // disabled with a helpful static_assert message.
  template <typename UnusedType = void>
  void set_on_client_stream_end(
      [[maybe_unused]] Function<void()> on_client_stream_end) {
    static_assert(
        cfg::kClientStreamEndCallbackEnabled<UnusedType>,
        "The client stream end callback is disabled, so "
        "set_on_client_stream_end cannot be called. To enable the client end "
        "callback, set PW_RPC_CLIENT_STREAM_END_CALLBACK to 1.");
#if PW_RPC_CLIENT_STREAM_END_CALLBACK
    on_client_stream_end_ = std::move(on_client_stream_end);
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK
  }

 private:
#if PW_RPC_CLIENT_STREAM_END_CALLBACK
  // Called when a client stream completes.
  Function<void()> on_client_stream_end_;
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK
};

// A Call object, as used by an RPC client.
class ClientCall : public Call {
 public:
  ~ClientCall() {
    if (active()) {
      Close();
    }
  }

  void SendInitialRequest(ConstByteSpan payload);

 protected:
  constexpr ClientCall() = default;

  ClientCall(Endpoint& client,
             uint32_t channel_id,
             uint32_t service_id,
             uint32_t method_id,
             MethodType type)
      : Call(client, channel_id, service_id, method_id, type) {}
};

// Unary response client calls receive both a payload and the status in their
// on_completed callback. The on_next callback is not used.
class UnaryResponseClientCall : public ClientCall {
 public:
  template <typename CallType>
  static CallType Start(Endpoint& client,
                        uint32_t channel_id,
                        uint32_t service_id,
                        uint32_t method_id,
                        Function<void(ConstByteSpan, Status)> on_completed,
                        Function<void(Status)> on_error,
                        ConstByteSpan request) {
    CallType call(client, channel_id, service_id, method_id);

    call.set_on_completed(std::move(on_completed));
    call.set_on_error(std::move(on_error));

    call.SendInitialRequest(request);
    return call;
  }

  void HandleCompleted(ConstByteSpan response, Status status) {
    Close();

    if (on_completed_) {
      on_completed_(response, status);
    }
  }

 protected:
  constexpr UnaryResponseClientCall() = default;

  UnaryResponseClientCall(Endpoint& client,
                          uint32_t channel_id,
                          uint32_t service_id,
                          uint32_t method_id,
                          MethodType type)
      : ClientCall(client, channel_id, service_id, method_id, type) {}

  void set_on_completed(Function<void(ConstByteSpan, Status)> on_completed) {
    on_completed_ = std::move(on_completed);
  }

  UnaryResponseClientCall(UnaryResponseClientCall&& other) {
    *this = std::move(other);
  }

  UnaryResponseClientCall& operator=(UnaryResponseClientCall&& other);

 private:
  using internal::ClientCall::set_on_next;  // Not used in unary response calls.

  Function<void(ConstByteSpan, Status)> on_completed_;
};

// Stream response client calls only receive the status in their on_completed
// callback. Payloads are sent through the on_next callback.
class StreamResponseClientCall : public ClientCall {
 public:
  template <typename CallType>
  static CallType Start(Endpoint& client,
                        uint32_t channel_id,
                        uint32_t service_id,
                        uint32_t method_id,
                        Function<void(ConstByteSpan)> on_next,
                        Function<void(Status)> on_completed,
                        Function<void(Status)> on_error,
                        ConstByteSpan request) {
    CallType call(client, channel_id, service_id, method_id);

    call.set_on_next(std::move(on_next));
    call.set_on_completed(std::move(on_completed));
    call.set_on_error(std::move(on_error));

    call.SendInitialRequest(request);
    return call;
  }

  void HandleCompleted(Status status) {
    Close();

    if (on_completed_) {
      on_completed_(status);
    }
  }

 protected:
  constexpr StreamResponseClientCall() = default;

  StreamResponseClientCall(Endpoint& client,
                           uint32_t channel_id,
                           uint32_t service_id,
                           uint32_t method_id,
                           MethodType type)
      : ClientCall(client, channel_id, service_id, method_id, type) {}

  StreamResponseClientCall(StreamResponseClientCall&& other) {
    *this = std::move(other);
  }

  StreamResponseClientCall& operator=(StreamResponseClientCall&& other);

  void set_on_completed(Function<void(Status)> on_completed) {
    on_completed_ = std::move(on_completed);
  }

 private:
  Function<void(Status)> on_completed_;
};

}  // namespace pw::rpc::internal
