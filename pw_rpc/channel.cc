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

// clang-format off
#include "pw_rpc/internal/log_config.h"  // PW_LOG_* macros must be first.

#include "pw_rpc/internal/channel.h"
// clang-format on

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
    PW_LOG_ERROR(
        "Failed to encode RPC packet type %u to channel %u buffer, status %u",
        static_cast<unsigned>(packet.type()),
        static_cast<unsigned>(id()),
        encoded.status().code());
    output().DiscardBuffer(buffer.buffer_);
    buffer.buffer_ = {};
    return Status::Internal();
  }

  buffer.buffer_ = {};
  Status status = output().SendAndReleaseBuffer(encoded.value());

  if (!status.ok()) {
    PW_LOG_DEBUG("Channel %u failed to send packet with status %u",
                 static_cast<unsigned>(id()),
                 status.code());

    // TODO(pwbug/503): It is important that pw_rpc provide a consistent set of
    //     status codes in its APIs. This status comes from a user class and
    //     should not be returned directly unless it maps to a standardized
    //     code. For now, just remap FAILED_PRECONDITION because that value is
    //     used within the RPC system for another purpose (attempted to use a
    //     closed RPC call object). Long term, the statuses need to be
    //     standardized across all APIs. For example, this might return OK,
    //     UNAVAILABLE, or DATA_LOSS and other codes are mapped to UNKNOWN.
    if (status.IsFailedPrecondition()) {
      status = Status::Unknown();
    }
  }
  return status;
}

}  // namespace pw::rpc::internal
