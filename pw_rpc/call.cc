// Copyright 2020 The Pigweed Authors
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

#include "pw_rpc/internal/call.h"

#include "pw_assert/check.h"
#include "pw_rpc/client.h"
#include "pw_rpc/internal/endpoint.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/server.h"

namespace pw::rpc::internal {

// Creates an active client-side call, assigning it a new ID.
Call::Call(Endpoint& client,
           uint32_t channel_id,
           uint32_t service_id,
           uint32_t method_id,
           MethodType type)
    : Call(client,
           client.NewCallId(),
           channel_id,
           service_id,
           method_id,
           type,
           kClientCall) {}

Call::Call(Endpoint& endpoint_ref,
           uint32_t call_id,
           uint32_t channel_id,
           uint32_t service_id,
           uint32_t method_id,
           MethodType type,
           CallType call_type)
    : endpoint_(&endpoint_ref),
      channel_(endpoint().GetInternalChannel(channel_id)),
      id_(call_id),
      service_id_(service_id),
      method_id_(method_id),
      rpc_state_(kActive),
      type_(type),
      call_type_(call_type),
      client_stream_state_(HasClientStream(type) ? kClientStreamActive
                                                 : kClientStreamInactive) {
  // TODO(pwbug/505): Defer channel lookup until it's needed to support dynamic
  // registration/removal of channels.
  PW_CHECK_NOTNULL(channel_,
                   "An RPC call was created for channel %u, but that channel "
                   "is not known to the server/client.",
                   static_cast<unsigned>(channel_id));

  endpoint().RegisterCall(*this);
}

void Call::MoveFrom(Call& other) {
  PW_DCHECK(!active());

  if (!other.active()) {
    return;  // Nothing else to do; this call is already closed.
  }

  // Copy all members from the other call.
  endpoint_ = other.endpoint_;
  channel_ = other.channel_;
  id_ = other.id_;
  service_id_ = other.service_id_;
  method_id_ = other.method_id_;

  rpc_state_ = other.rpc_state_;
  type_ = other.type_;
  call_type_ = other.call_type_;
  client_stream_state_ = other.client_stream_state_;

  response_ = std::move(other.response_);

  on_error_ = std::move(other.on_error_);
  on_next_ = std::move(other.on_next_);

  // Mark the other call inactive, unregister it, and register this one.
  other.rpc_state_ = kInactive;
  other.client_stream_state_ = kClientStreamInactive;

  endpoint().UnregisterCall(other);
  endpoint().RegisterUniqueCall(*this);
}

Status Call::EndClientStream() {
  client_stream_state_ = kClientStreamInactive;
  return SendPacket(PacketType::CLIENT_STREAM_END, {}, {});
}

Status Call::CloseAndSendFinalPacket(PacketType type,
                                     ConstByteSpan response,
                                     Status status) {
  rpc_lock().lock();
  if (!active()) {
    rpc_lock().unlock();
    return Status::FailedPrecondition();
  }
  Close();
  return SendPacket(type, response, status);
}

ByteSpan Call::PayloadBuffer() {
  // Only allow having one active buffer at a time.
  if (response_.empty()) {
    response_ = channel().AcquireBuffer();
  }

  // The packet type is only used to size the payload buffer.
  // TODO(pwrev/506): Replace the packet header calculation with a constant
  //     rather than creating a packet.
  return response_.payload(MakePacket(PacketType::CLIENT_STREAM, {}));
}

Status Call::Write(ConstByteSpan payload) {
  rpc_lock().lock();
  if (!active()) {
    rpc_lock().unlock();
    return Status::FailedPrecondition();
  }
  return SendPacket(call_type_ == kServerCall ? PacketType::SERVER_STREAM
                                              : PacketType::CLIENT_STREAM,
                    payload);
}

Status Call::SendPacket(PacketType type, ConstByteSpan payload, Status status) {
  const Packet packet = MakePacket(type, payload, status);

  if (!buffer().Contains(payload)) {
    ByteSpan buffer = PayloadBuffer();

    if (payload.size() > buffer.size()) {
      ReleasePayloadBuffer();
      rpc_lock().unlock();
      return Status::OutOfRange();
    }

    std::copy_n(payload.data(), payload.size(), buffer.data());
  }

  rpc_lock().unlock();
  return channel().Send(response_, packet);
}

void Call::ReleasePayloadBuffer() {
  PW_DCHECK(active());
  channel().Release(response_);
}

void Call::Close() {
  if (active()) {
    endpoint().UnregisterCall(*this);
  }

  rpc_state_ = kInactive;
  client_stream_state_ = kClientStreamInactive;
}

}  // namespace pw::rpc::internal
