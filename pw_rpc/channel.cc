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

#include "pw_rpc/internal/channel.h"

#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc::internal {

using std::byte;

std::span<byte> Channel::OutputBuffer::payload(const Packet& packet) const {
  const size_t reserved_size = packet.MinEncodedSizeBytes();
  return reserved_size <= buffer_.size() ? buffer_.subspan(reserved_size)
                                         : std::span<byte>();
}

Status Channel::Send(OutputBuffer& buffer, const internal::Packet& packet) {
  Result encoded = packet.Encode(buffer.buffer_);

  if (!encoded.ok()) {
    PW_LOG_ERROR("Failed to encode response packet to channel buffer");
    output().DiscardBuffer(buffer.buffer_);
    buffer.buffer_ = {};
    return Status::Internal();
  }

  buffer.buffer_ = {};
  return output().SendAndReleaseBuffer(encoded.value());
}

}  // namespace pw::rpc::internal
