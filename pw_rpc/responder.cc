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

#include "pw_rpc/internal/responder.h"

#include "pw_assert/check.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/server.h"

namespace pw::rpc::internal {
namespace {

Packet ResponsePacket(const ServerCall& call,
                      std::span<const std::byte> payload,
                      Status status) {
  return Packet(PacketType::RESPONSE,
                call.channel().id(),
                call.service().id(),
                call.method().id(),
                payload,
                status);
}

Packet StreamPacket(const ServerCall& call,
                    std::span<const std::byte> payload) {
  return Packet(PacketType::SERVER_STREAM,
                call.channel().id(),
                call.service().id(),
                call.method().id(),
                payload);
}

}  // namespace

Responder::Responder(ServerCall& call, HasClientStream has_client_stream)
    : call_(call),
      rpc_state_(kOpen),
      has_client_stream_(has_client_stream),
      client_stream_state_(has_client_stream ? kClientStreamOpen
                                             : kClientStreamClosed) {
  call_.server().RegisterResponder(*this);
}

Responder& Responder::operator=(Responder&& other) {
  CloseAndSendResponse(OkStatus()).IgnoreError();

  // Move the state variables, which may change when the other client closes.
  rpc_state_ = other.rpc_state_;
  has_client_stream_ = other.has_client_stream_;
  client_stream_state_ = other.client_stream_state_;

  if (other.open()) {
    other.Close();
    other.call_.server().RegisterResponder(*this);
  }

  // Move the rest of the member variables.
  call_ = std::move(other.call_);
  response_ = std::move(other.response_);

  on_error_ = std::move(other.on_error_);
  on_next_ = std::move(other.on_next_);

#if PW_RPC_CLIENT_STREAM_END_CALLBACK
  on_client_stream_end_ = std::move(other.on_client_stream_end_);
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK

  return *this;
}

uint32_t Responder::method_id() const { return call_.method().id(); }

Status Responder::CloseAndSendResponse(std::span<const std::byte> response,
                                       Status status) {
  if (!open()) {
    return Status::FailedPrecondition();
  }

  // Send a packet indicating that the RPC has terminated.
  Status packet_status =
      call_.channel().Send(ResponsePacket(call_, response, status));

  // If the Responder implementer or user forgets to release an acquired buffer
  // before finishing, release it here.
  if (!response_.empty()) {
    ReleasePayloadBuffer();
  }

  Close();

  return packet_status;
}

std::span<std::byte> Responder::AcquirePayloadBuffer() {
  PW_DCHECK(open());

  // Only allow having one active buffer at a time.
  if (response_.empty()) {
    response_ = call_.channel().AcquireBuffer();
  }

  return response_.payload(StreamPacket(call_, {}));
}

Status Responder::ReleasePayloadBuffer(std::span<const std::byte> payload) {
  PW_DCHECK(open());
  return call_.channel().Send(response_, StreamPacket(call_, payload));
}

Status Responder::ReleasePayloadBuffer() {
  PW_DCHECK(open());
  call_.channel().Release(response_);
  return OkStatus();
}

void Responder::Close() {
  PW_DCHECK(open());

  call_.server().RemoveResponder(*this);
  rpc_state_ = kClosed;
  client_stream_state_ = kClientStreamClosed;
}

}  // namespace pw::rpc::internal
