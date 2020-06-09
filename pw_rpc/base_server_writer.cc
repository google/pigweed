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

#include "pw_assert/assert.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/server.h"

namespace pw::rpc::internal {

BaseServerWriter& BaseServerWriter::operator=(BaseServerWriter&& other) {
  context_ = std::move(other.context_);
  response_ = std::move(other.response_);
  state_ = std::move(other.state_);
  other.state_ = kClosed;
  return *this;
}

void BaseServerWriter::close() {
  if (open()) {
    // TODO(hepler): Send a control packet indicating that the stream has
    // terminated, and remove this ServerWriter from the Server's list.

    state_ = kClosed;
  }
}

span<std::byte> BaseServerWriter::AcquireBuffer() {
  if (!open()) {
    return {};
  }

  PW_DCHECK(response_.empty());
  response_ = context_.channel().AcquireBuffer();

  // Reserve space for the RPC packet header.
  return packet().PayloadUsableSpace(response_);
}

Status BaseServerWriter::SendAndReleaseBuffer(span<const std::byte> payload) {
  if (!open()) {
    return Status::FAILED_PRECONDITION;
  }

  Packet response_packet = packet();
  response_packet.set_payload(payload);
  StatusWithSize encoded = response_packet.Encode(response_);
  response_ = {};

  if (!encoded.ok()) {
    context_.channel().SendAndReleaseBuffer(0);
    return Status::INTERNAL;
  }

  // TODO(hepler): Should Channel::SendAndReleaseBuffer return Status?
  context_.channel().SendAndReleaseBuffer(encoded.size());
  return Status::OK;
}

Packet BaseServerWriter::packet() const {
  return Packet(PacketType::RPC,
                context_.channel_id(),
                context_.service().id(),
                method().id());
}

}  // namespace pw::rpc::internal
