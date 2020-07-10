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

#include "pw_rpc/internal/base_server_writer.h"

#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/server.h"

namespace pw::rpc::internal {

BaseServerWriter::BaseServerWriter(ServerCall& call)
    : call_(call), state_(kOpen) {
  call_.server().RegisterWriter(*this);
}

BaseServerWriter& BaseServerWriter::operator=(BaseServerWriter&& other) {
  Finish();

  state_ = other.state_;

  if (other.open()) {
    other.call_.server().RemoveWriter(other);
    other.state_ = kClosed;

    other.call_.server().RegisterWriter(*this);
  }

  call_ = std::move(other.call_);
  response_ = std::move(other.response_);

  return *this;
}

void BaseServerWriter::Finish() {
  if (!open()) {
    return;
  }

  call_.server().RemoveWriter(*this);

  // TODO(hepler): Send a control packet indicating that the stream has
  // terminated.

  state_ = kClosed;
}

std::span<std::byte> BaseServerWriter::AcquirePayloadBuffer() {
  if (!open()) {
    return {};
  }

  response_ = call_.channel().AcquireBuffer();
  return response_.payload(packet());
}

Status BaseServerWriter::ReleasePayloadBuffer(
    std::span<const std::byte> payload) {
  if (!open()) {
    return Status::FAILED_PRECONDITION;
  }
  return call_.channel().Send(response_, packet(payload));
}

Packet BaseServerWriter::packet(std::span<const std::byte> payload) const {
  return Packet(PacketType::RPC,
                call_.channel().id(),
                call_.service().id(),
                method().id(),
                payload);
}

}  // namespace pw::rpc::internal
