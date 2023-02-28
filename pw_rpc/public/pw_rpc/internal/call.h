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

#include <cassert>
#include <cstddef>
#include <limits>
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
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"

namespace pw::rpc {

class Writer;

namespace internal {

class Endpoint;
class LockedEndpoint;
class Packet;

// Whether a call object is associated with a server or a client.
enum CallType : bool { kServerCall, kClientCall };

// Whether callbacks that take a proto use the raw data directly or decode it
// to a struct. The RPC lock is held when invoking callbacks that decode to a
// struct.
enum CallbackProtoType : bool { kRawProto, kProtoStruct };

// Immutable properties of a call object. These do not change after an active
// call is initialized.
//
// Bits
//     0-1: MethodType
//       2: CallType
//       3: Bool for whether callbacks decode to proto structs
//
class CallProperties {
 public:
  constexpr CallProperties() : bits_(0u) {}

  constexpr CallProperties(MethodType method_type,
                           CallType call_type,
                           CallbackProtoType callback_proto_type)
      : bits_((static_cast<uint8_t>(method_type) << 0) |
              (static_cast<uint8_t>(call_type) << 2) |
              (static_cast<uint8_t>(callback_proto_type) << 3)) {}

  constexpr CallProperties(const CallProperties&) = default;

  constexpr CallProperties& operator=(const CallProperties&) = default;

  constexpr MethodType method_type() const {
    return static_cast<MethodType>(bits_ & 0b0011u);
  }

  constexpr CallType call_type() const {
    return static_cast<CallType>((bits_ & 0b0100u) >> 2);
  }

  constexpr CallbackProtoType callback_proto_type() const {
    return static_cast<CallbackProtoType>((bits_ & 0b1000u) >> 3);
  }

 private:
  uint8_t bits_;
};

// Unrequested RPCs always use this call ID. When a subsequent request
// or response is sent with a matching channel + service + method,
// it will match a calls with this ID if one exists.
inline constexpr uint32_t kOpenCallId = std::numeric_limits<uint32_t>::max();

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

  ~Call() PW_LOCKS_EXCLUDED(rpc_lock());

  // True if the Call is active and ready to send responses.
  [[nodiscard]] bool active() const PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return active_locked();
  }

  [[nodiscard]] bool active_locked() const
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return (state_ & kActive) != 0;
  }

  [[nodiscard]] bool awaiting_cleanup() const
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return awaiting_cleanup_ != OkStatus().code();
  }

  uint32_t id() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) { return id_; }

  void set_id(uint32_t id) PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) { id_ = id; }

  // Public function for accessing the channel ID of this call. Set to 0 when
  // the call is closed.
  uint32_t channel_id() const PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return channel_id_locked();
  }

  uint32_t channel_id_locked() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return channel_id_;
  }

  uint32_t service_id() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return service_id_;
  }

  uint32_t method_id() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return method_id_;
  }

  // Return whether this is a server or client call.
  CallType type() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return properties_.call_type();
  }

  // Closes the Call and sends a RESPONSE packet, if it is active. Returns the
  // status from sending the packet, or FAILED_PRECONDITION if the Call is not
  // active.
  Status CloseAndSendResponse(ConstByteSpan response, Status status)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return CloseAndSendResponseLocked(response, status);
  }

  Status CloseAndSendResponseLocked(ConstByteSpan response, Status status)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return CloseAndSendFinalPacketLocked(
        pwpb::PacketType::RESPONSE, response, status);
  }

  Status CloseAndSendResponse(Status status) PW_LOCKS_EXCLUDED(rpc_lock()) {
    return CloseAndSendResponse({}, status);
  }

  Status CloseAndSendServerErrorLocked(Status error)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return CloseAndSendFinalPacketLocked(
        pwpb::PacketType::SERVER_ERROR, {}, error);
  }

  // Public function that ends the client stream for a client call.
  Status CloseClientStream() PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return CloseClientStreamLocked();
  }

  // Internal function that closes the client stream.
  Status CloseClientStreamLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    MarkClientStreamCompleted();
    return SendPacket(pwpb::PacketType::CLIENT_STREAM_END, {}, {});
  }

  // Sends a payload in either a server or client stream packet.
  Status Write(ConstByteSpan payload) PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return WriteLocked(payload);
  }

  Status WriteLocked(ConstByteSpan payload)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  // Sends the initial request for a client call. If the request fails, the call
  // is closed.
  void SendInitialClientRequest(ConstByteSpan payload)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    if (const Status status = SendPacket(pwpb::PacketType::REQUEST, payload);
        !status.ok()) {
      CloseAndMarkForCleanup(status);
    }
  }

  void CloseAndMarkForCleanup(Status error)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  // Whenever a payload arrives (in a server/client stream or in a response),
  // call the on_next_ callback.
  // Precondition: rpc_lock() must be held.
  void HandlePayload(ConstByteSpan payload) PW_UNLOCK_FUNCTION(rpc_lock());

  // Handles an error condition for the call. This closes the call and calls the
  // on_error callback, if set.
  void HandleError(Status status) PW_UNLOCK_FUNCTION(rpc_lock()) {
    UnregisterAndMarkClosed();
    CallOnError(status);
  }

  // Closes the RPC, but does NOT unregister the call or call on_error. The
  // call must be moved to the endpoint's to_cleanup_ list and have its
  // CleanUp() method called at a later time. Only for use by the Endpoint.
  void CloseAndMarkForCleanupFromEndpoint(Status error)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    MarkClosed();
    awaiting_cleanup_ = error.code();
  }

  // Clears the awaiting_cleanup_ variable and calls the on_error callback. Only
  // for use by the Endpoint, which will unlist the call.
  void CleanUpFromEndpoint() PW_UNLOCK_FUNCTION(rpc_lock()) {
    const Status status(static_cast<Status::Code>(awaiting_cleanup_));
    awaiting_cleanup_ = OkStatus().code();
    CallOnError(status);
  }

  bool has_client_stream() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return HasClientStream(properties_.method_type());
  }

  bool has_server_stream() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return HasServerStream(properties_.method_type());
  }

  bool client_stream_open() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return (state_ & kClientStreamActive) != 0;
  }

  // Closes a call without doing anything else. Called from the Endpoint
  // destructor.
  void CloseFromDeletedEndpoint() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    MarkClosed();
    awaiting_cleanup_ = OkStatus().code();
    endpoint_ = nullptr;
  }

 protected:
  // Creates an inactive Call.
  constexpr Call()
      : endpoint_{},
        channel_id_{},
        id_{},
        service_id_{},
        method_id_{},
        state_{},
        awaiting_cleanup_{},
        callbacks_executing_{},
        properties_{} {}

  // Creates an active server-side Call.
  Call(const LockedCallContext& context, CallProperties properties)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  // Creates an active client-side Call.
  Call(LockedEndpoint& client,
       uint32_t channel_id,
       uint32_t service_id,
       uint32_t method_id,
       CallProperties properties) PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  void CallbackStarted() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    callbacks_executing_ += 1;
  }

  void CallbackFinished() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    callbacks_executing_ -= 1;
  }

  // This call must be in a closed state when this is called.
  void MoveFrom(Call& other) PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  Endpoint& endpoint() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return *endpoint_;
  }

  // Public function that sets the on_next function in the raw API.
  void set_on_next(Function<void(ConstByteSpan)>&& on_next)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    set_on_next_locked(std::move(on_next));
  }

  // Internal function that sets on_next.
  void set_on_next_locked(Function<void(ConstByteSpan)>&& on_next)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    on_next_ = std::move(on_next);
  }

  // Public function that sets the on_error callback.
  void set_on_error(Function<void(Status)>&& on_error)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    set_on_error_locked(std::move(on_error));
  }

  // Internal function that sets on_error.
  void set_on_error_locked(Function<void(Status)>&& on_error)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    on_error_ = std::move(on_error);
  }

  void MarkClientStreamCompleted() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    state_ &= ~kClientStreamActive;
  }

  Status CloseAndSendResponseLocked(Status status)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return CloseAndSendFinalPacketLocked(
        pwpb::PacketType::RESPONSE, {}, status);
  }

  // Cancels an RPC. Public function for client calls only.
  Status Cancel() PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return CloseAndSendFinalPacketLocked(
        pwpb::PacketType::CLIENT_ERROR, {}, Status::Cancelled());
  }

  // Unregisters the RPC from the endpoint & marks as closed. The call may be
  // active or inactive when this is called.
  void UnregisterAndMarkClosed() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  // Define conversions to the generic server/client RPC writer class. These
  // functions are defined in pw_rpc/writer.h after the Writer class is defined.
  constexpr operator Writer&();
  constexpr operator const Writer&() const;

  // Indicates if the on_next and unary on_completed callbacks are internal
  // wrappers that decode the raw proto before invoking the user's callback. If
  // they are, the lock must be held when they are invoked.
  bool hold_lock_while_invoking_callback_with_payload() const
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return properties_.callback_proto_type() == kProtoStruct;
  }

  // Decodes a raw protobuf into a proto struct (pwpb or Nanopb) and invokes the
  // pwpb or Nanopb version of the on_next callback.
  //
  // This must ONLY be called from derived classes the wrap the on_next
  // callback. These classes MUST indicate that they call calls in their
  // constructor.
  template <typename Decoder, typename ProtoStruct>
  void DecodeToStructAndInvokeOnNext(
      ConstByteSpan payload,
      const Decoder& decoder,
      Function<void(const ProtoStruct&)>& proto_on_next)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    if (proto_on_next == nullptr) {
      return;
    }

    ProtoStruct proto_struct{};

    if (!decoder.Decode(payload, proto_struct).ok()) {
      CloseAndMarkForCleanup(Status::DataLoss());
      return;
    }

    const uint32_t original_id = id();
    auto proto_on_next_local = std::move(proto_on_next);

    rpc_lock().unlock();
    proto_on_next_local(proto_struct);
    rpc_lock().lock();

    // Restore the original callback if the original call is still active and
    // the callback has not been replaced.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    if (active_locked() && id() == original_id && proto_on_next == nullptr) {
      proto_on_next = std::move(proto_on_next_local);
    }
  }

  // The call is already unregistered and closed.
  template <typename Decoder, typename ProtoStruct>
  void DecodeToStructAndInvokeOnCompleted(
      ConstByteSpan payload,
      const Decoder& decoder,
      Function<void(const ProtoStruct&, Status)>& proto_on_completed,
      Status status) PW_UNLOCK_FUNCTION(rpc_lock()) {
    // Always move proto_on_completed so it goes out of scope in this function.
    auto proto_on_completed_local = std::move(proto_on_completed);

    // Move on_error in case an error occurs.
    auto on_error_local = std::move(on_error_);

    // Release the lock before decoding, since decoder is a global.
    rpc_lock().unlock();

    if (proto_on_completed_local == nullptr) {
      return;
    }

    ProtoStruct proto_struct{};
    if (decoder.Decode(payload, proto_struct).ok()) {
      proto_on_completed_local(proto_struct, status);
    } else if (on_error_local != nullptr) {
      on_error_local(Status::DataLoss());
    }
  }

  // An active call cannot be moved if its callbacks are running. This function
  // must be called on the call being moved before updating any state.
  static void WaitUntilReadyForMove(Call& destination, Call& source)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

 private:
  enum State : uint8_t {
    kActive = 0b01,
    kClientStreamActive = 0b10,
  };

  // Common constructor for server & client calls.
  Call(LockedEndpoint& endpoint,
       uint32_t id,
       uint32_t channel_id,
       uint32_t service_id,
       uint32_t method_id,
       CallProperties properties);

  Packet MakePacket(pwpb::PacketType type,
                    ConstByteSpan payload,
                    Status status = OkStatus()) const
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return Packet(type,
                  channel_id_locked(),
                  service_id(),
                  method_id(),
                  id_,
                  payload,
                  status);
  }

  // Marks a call object closed without doing anything else. The call is not
  // removed from the calls list and no callbacks are called.
  void MarkClosed() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    channel_id_ = Channel::kUnassignedChannelId;
    id_ = 0;
    state_ = 0;
  }

  // Calls the on_error callback without closing the RPC. This is used when the
  // call has already completed.
  void CallOnError(Status error) PW_UNLOCK_FUNCTION(rpc_lock());

  // If required, removes this call from the endpoint's to_cleanup_ list and
  // calls CleanUp(). Returns true if cleanup was required, which means the lock
  // was released.
  bool CleanUpIfRequired() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  // Sends a payload with the specified type. The payload may either be in a
  // previously acquired buffer or in a standalone buffer.
  //
  // Returns FAILED_PRECONDITION if the call is not active().
  Status SendPacket(pwpb::PacketType type,
                    ConstByteSpan payload,
                    Status status = OkStatus())
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  Status CloseAndSendFinalPacketLocked(pwpb::PacketType type,
                                       ConstByteSpan response,
                                       Status status)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  bool CallbacksAreRunning() const PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return callbacks_executing_ != 0u;
  }

  Endpoint* endpoint_ PW_GUARDED_BY(rpc_lock());
  uint32_t channel_id_ PW_GUARDED_BY(rpc_lock());
  uint32_t id_ PW_GUARDED_BY(rpc_lock());
  uint32_t service_id_ PW_GUARDED_BY(rpc_lock());
  uint32_t method_id_ PW_GUARDED_BY(rpc_lock());

  // State of call and client stream.
  //
  //   bit 0: call is active
  //   bit 1: client stream is active
  //
  uint8_t state_ PW_GUARDED_BY(rpc_lock());

  // If non-OK, indicates that the call was closed and needs to have its
  // on_error called with this Status code. Uses a uint8_t for compactness.
  uint8_t awaiting_cleanup_ PW_GUARDED_BY(rpc_lock());

  // Tracks how many of this call's callbacks are running. Must be 0 for the
  // call to be destroyed.
  uint8_t callbacks_executing_ PW_GUARDED_BY(rpc_lock());

  CallProperties properties_ PW_GUARDED_BY(rpc_lock());

  // Called when the RPC is terminated due to an error.
  Function<void(Status error)> on_error_ PW_GUARDED_BY(rpc_lock());

  // Called when a request is received. Only used for RPCs with client streams.
  // The raw payload buffer is passed to the callback.
  Function<void(ConstByteSpan payload)> on_next_ PW_GUARDED_BY(rpc_lock());
};

}  // namespace internal
}  // namespace pw::rpc
