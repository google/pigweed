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
#include "pw_rpc/internal/endpoint.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/server.h"

namespace pw::rpc::internal {

Call::Call(const CallContext& call, MethodType type)
    : endpoint_(&call.endpoint()),
      channel_(&call.channel()),
      service_id_(call.service().id()),
      method_id_(call.method().id()),
      rpc_state_(kActive),
      type_(type),
      client_stream_state_(HasClientStream(type) ? kClientStreamActive
                                                 : kClientStreamInactive) {
  endpoint().RegisterCall(*this);
}

void Call::MoveFrom(Call& other) {
  // If this RPC was running, complete it before moving in the other RPC.
  CloseAndSendResponse(OkStatus()).IgnoreError();

  // Move the state variables, which may change when the other client closes.
  rpc_state_ = other.rpc_state_;
  type_ = other.type_;
  client_stream_state_ = other.client_stream_state_;

  endpoint_ = other.endpoint_;
  channel_ = other.channel_;
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
                                     std::span<const std::byte> payload,
                                     Status status) {
  if (!active()) {
    return Status::FailedPrecondition();
  }

  Status packet_status;

  // Acquire a buffer to use for the outgoing packet if none is available.
  if (response_.empty()) {
    response_ = channel().AcquireBuffer();
  }

  // Send a packet indicating that the RPC has terminated and optionally
  // containing the final payload.
  packet_status = channel().Send(
      response_,
      Packet(type, channel_id(), service_id(), method_id(), payload, status));

  Close();

  return packet_status;
}

std::span<std::byte> Call::AcquirePayloadBuffer() {
  PW_DCHECK(active());

  // Only allow having one active buffer at a time.
  if (response_.empty()) {
    response_ = channel().AcquireBuffer();
  }

  return response_.payload(StreamPacket({}));
}

Status Call::SendPayloadBufferClientStream(std::span<const std::byte> payload) {
  PW_DCHECK(active());
  return channel().Send(response_, StreamPacket(payload));
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

ClientCall& ClientCall::operator=(ClientCall&& other) {
  MoveFrom(other);

  on_completed_ = std::move(other.on_completed_);

  return *this;
}

}  // namespace pw::rpc::internal
