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
#include "pw_rpc/internal/lock.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/method_type.h"
#include "pw_rpc/service.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"

namespace pw::rpc {

class Writer;

namespace internal {

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

  uint32_t id() const { return id_; }

  uint32_t channel_id() const {
    return channel_ == nullptr ? Channel::kUnassignedChannelId : channel().id();
  }
  uint32_t service_id() const { return service_id_; }
  uint32_t method_id() const { return method_id_; }

  // Closes the Call and sends a RESPONSE packet, if it is active. Returns the
  // status from sending the packet, or FAILED_PRECONDITION if the Call is not
  // active.
  Status CloseAndSendResponse(ConstByteSpan response, Status status)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    return CloseAndSendFinalPacket(PacketType::RESPONSE, response, status);
  }

  Status CloseAndSendResponse(Status status) PW_LOCKS_EXCLUDED(rpc_lock()) {
    return CloseAndSendResponse({}, status);
  }

  Status CloseAndSendServerError(Status error) PW_LOCKS_EXCLUDED(rpc_lock()) {
    return CloseAndSendFinalPacket(PacketType::SERVER_ERROR, {}, error);
  }

  Status CloseAndSendClientError(Status error) PW_LOCKS_EXCLUDED(rpc_lock()) {
    return CloseAndSendFinalPacket(PacketType::CLIENT_ERROR, {}, error);
  }

  // Ends the client stream for a client call.
  Status EndClientStream() PW_UNLOCK_FUNCTION(rpc_lock());

  // Sends a payload in either a server or client stream packet.
  Status Write(ConstByteSpan payload) PW_LOCKS_EXCLUDED(rpc_lock());

  // Whenever a payload arrives (in a server/client stream or in a response),
  // call the on_next_ callback.
  // Precondition: rpc_lock() must be held.
  void HandlePayload(ConstByteSpan message) const
      PW_UNLOCK_FUNCTION(rpc_lock()) {
    const bool invoke = on_next_ != nullptr;
    rpc_lock().unlock();

    if (invoke) {
      on_next_(message);
    }
  }

  // Precondition: rpc_lock() must be held.
  void HandleError(Status status) PW_UNLOCK_FUNCTION(rpc_lock()) {
    Close();
    on_error(status);
  }

  bool has_client_stream() const { return HasClientStream(type_); }
  bool has_server_stream() const { return HasServerStream(type_); }

  bool client_stream_open() const {
    return client_stream_state_ == kClientStreamActive;
  }

  // Acquires a buffer into which to write a payload or returns a previously
  // acquired buffer. The Call MUST be active when this is called!
  ByteSpan PayloadBuffer();

  // Releases the buffer without sending a packet.
  void ReleasePayloadBuffer();

  // Keep this public so the Nanopb implementation can set it from a helper
  // function.
  void set_on_next(Function<void(ConstByteSpan)>&& on_next)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    LockGuard lock{rpc_lock()};
    set_on_next_locked(std::move(on_next));
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
        client_stream_state_ {}
  {}

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

  // This call must be in a closed state when this is called.
  void MoveFrom(Call& other) PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  Endpoint& endpoint() const { return *endpoint_; }
  Channel& channel() const { return *channel_; }

  void set_on_next_locked(Function<void(ConstByteSpan)>&& on_next) {
    on_next_ = std::move(on_next);
  }

  void set_on_error(Function<void(Status)>&& on_error) {
    on_error_ = std::move(on_error);
  }

  // Calls the on_error callback without closing the RPC. This is used when the
  // call has already completed.
  void on_error(Status error) PW_UNLOCK_FUNCTION(rpc_lock()) {
    const bool invoke = on_error_ != nullptr;

    rpc_lock().unlock();
    if (invoke) {
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
                    Status status = OkStatus()) PW_UNLOCK_FUNCTION(rpc_lock());

  // Unregisters the RPC from the endpoint & marks as closed. The call may be
  // active or inactive when this is called.
  void Close() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  // Cancels an RPC. For client calls only.
  Status Cancel() PW_LOCKS_EXCLUDED(rpc_lock()) {
    return CloseAndSendFinalPacket(
        PacketType::CLIENT_ERROR, {}, Status::Cancelled());
  }

  // Define conversions to the generic server/client RPC writer class. These
  // functions are defined in pw_rpc/writer.h after the Writer class is defined.
  constexpr operator Writer&();
  constexpr operator const Writer&() const;

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
                                 Status status) PW_LOCKS_EXCLUDED(rpc_lock());

  internal::Endpoint* endpoint_;
  internal::Channel* channel_;
  uint32_t id_;
  uint32_t service_id_;
  uint32_t method_id_;

  enum : bool { kInactive, kActive } rpc_state_;
  MethodType type_;
  CallType call_type_;
  enum : bool {
    kClientStreamInactive,
    kClientStreamActive,
  } client_stream_state_;

  Channel::OutputBuffer response_;

  // Called when the RPC is terminated due to an error.
  Function<void(Status error)> on_error_;

  // Called when a request is received. Only used for RPCs with client streams.
  // The raw payload buffer is passed to the callback.
  Function<void(ConstByteSpan payload)> on_next_;
};

}  // namespace internal
}  // namespace pw::rpc
