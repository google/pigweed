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
                      uint32_t method_id,
                      Status status) {
  return Packet(PacketType::RESPONSE,
                call.channel().id(),
                call.service().id(),
                method_id,
                {},
                status);
}

Packet StreamPacket(const ServerCall& call,
                    uint32_t method_id,
                    std::span<const std::byte> payload) {
  return Packet(PacketType::SERVER_STREAM,
                call.channel().id(),
                call.service().id(),
                method_id,
                payload);
}

}  // namespace

Responder::Responder(ServerCall& call) : call_(call), state_(kOpen) {
  call_.server().RegisterResponder(*this);
}

Responder& Responder::operator=(Responder&& other) {
  Finish();

  state_ = other.state_;

  if (other.open()) {
    other.call_.server().RemoveResponder(other);
    other.state_ = kClosed;

    other.call_.server().RegisterResponder(*this);
  }

  call_ = std::move(other.call_);
  response_ = std::move(other.response_);

  return *this;
}

uint32_t Responder::method_id() const { return call_.method().id(); }

Status Responder::Finish(Status status) {
  if (!open()) {
    return Status::FailedPrecondition();
  }

  // If the Responder implementer or user forgets to release an acquired
  // buffer before finishing, release it here.
  if (!response_.empty()) {
    ReleasePayloadBuffer();
  }

  Close();

  // Send a packet indicating that the RPC has terminated.
  return call_.channel().Send(ResponsePacket(call_, method_id(), status));
}

std::span<std::byte> Responder::AcquirePayloadBuffer() {
  PW_DCHECK(open());

  // Only allow having one active buffer at a time.
  if (response_.empty()) {
    response_ = call_.channel().AcquireBuffer();
  }

  return response_.payload(StreamPacket(call_, method_id(), {}));
}

Status Responder::ReleasePayloadBuffer(std::span<const std::byte> payload) {
  PW_DCHECK(open());
  return call_.channel().Send(response_,
                              StreamPacket(call_, method_id(), payload));
}

Status Responder::ReleasePayloadBuffer() {
  PW_DCHECK(open());
  call_.channel().Release(response_);
  return OkStatus();
}

void Responder::Close() {
  if (!open()) {
    return;
  }

  call_.server().RemoveResponder(*this);
  state_ = kClosed;
}

}  // namespace pw::rpc::internal
