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
  // If this RPC was running, complete it before moving in the other RPC.
  CloseAndSendResponse(OkStatus()).IgnoreError();

  // Move the state variables, which may change when the other client closes.
  rpc_state_ = other.rpc_state_;
  type_ = other.type_;
  call_type_ = other.call_type_;
  client_stream_state_ = other.client_stream_state_;

  endpoint_ = other.endpoint_;
  channel_ = other.channel_;
  id_ = other.id_;
  service_id_ = other.service_id_;
  method_id_ = other.method_id_;

  if (other.active()) {
    other.Close();

    // This call is known to be unique since the other call was just closed.
    endpoint().RegisterUniqueCall(*this);
  }

  // Move the rest of the member variables.
  response_ = std::move(other.response_);

  on_error_ = std::move(other.on_error_);
  on_next_ = std::move(other.on_next_);
}

Status Call::CloseAndSendFinalPacket(PacketType type,
                                     ConstByteSpan payload,
                                     Status status) {
  if (!active()) {
    return Status::FailedPrecondition();
  }
  const Status packet_status = SendPacket(type, payload, status);
  Close();
  return packet_status;
}

ByteSpan Call::AcquirePayloadBuffer() {
  PW_DCHECK(active());

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
  if (!active()) {
    return Status::FailedPrecondition();
  }
  return SendPacket(call_type_ == kServerCall ? PacketType::SERVER_STREAM
                                              : PacketType::CLIENT_STREAM,
                    payload);
}

Status Call::SendPacket(PacketType type, ConstByteSpan payload, Status status) {
  PW_DCHECK(active());
  const Packet packet = MakePacket(type, payload, status);

  if (buffer().Contains(payload)) {
    return channel().Send(response_, packet);
  }

  ByteSpan buffer = AcquirePayloadBuffer();

  if (payload.size() > buffer.size()) {
    ReleasePayloadBuffer();
    return Status::OutOfRange();
  }

  std::memcpy(buffer.data(), payload.data(), payload.size());
  return channel().Send(response_, packet);
}

void Call::ReleasePayloadBuffer() {
  PW_DCHECK(active());
  channel().Release(response_);
}

void Call::Close() {
  PW_DCHECK(active());

  endpoint().UnregisterCall(*this);
  rpc_state_ = kInactive;
  client_stream_state_ = kClientStreamInactive;
}

ServerCall& ServerCall::operator=(ServerCall&& other) {
  MoveFrom(other);

#if PW_RPC_CLIENT_STREAM_END_CALLBACK
  on_client_stream_end_ = std::move(other.on_client_stream_end_);
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK

  return *this;
}

void ClientCall::SendInitialRequest(ConstByteSpan payload) {
  if (const Status status = SendPacket(PacketType::REQUEST, payload);
      !status.ok()) {
    HandleError(status);
  }
}

UnaryResponseClientCall& UnaryResponseClientCall::operator=(
    UnaryResponseClientCall&& other) {
  MoveFrom(other);
  on_completed_ = std::move(other.on_completed_);
  return *this;
}

StreamResponseClientCall& StreamResponseClientCall::operator=(
    StreamResponseClientCall&& other) {
  MoveFrom(other);
  on_completed_ = std::move(other.on_completed_);
  return *this;
}

}  // namespace pw::rpc::internal
