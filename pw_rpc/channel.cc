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
#include "pw_rpc/internal/config.h"

namespace pw::rpc::internal {

namespace {

std::array<std::byte, cfg::kEncodingBufferSizeBytes> encoding_buffer
    PW_GUARDED_BY(rpc_lock());

}

ByteSpan GetPayloadBuffer() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
  return ByteSpan(encoding_buffer)
      .subspan(Packet::kMinEncodedSizeWithoutPayload);
}

Status Channel::Send(const Packet& packet) {
  Result encoded = packet.Encode(encoding_buffer);

  if (!encoded.ok()) {
    PW_LOG_ERROR(
        "Failed to encode RPC packet type %u to channel %u buffer, status %u",
        static_cast<unsigned>(packet.type()),
        static_cast<unsigned>(id()),
        encoded.status().code());
    return Status::Internal();
  }

  Status sent = output().Send(encoded.value());

  if (!sent.ok()) {
    PW_LOG_DEBUG("Channel %u failed to send packet with status %u",
                 static_cast<unsigned>(id()),
                 sent.code());

    // TODO(pwbug/503): It is important that pw_rpc provide a consistent set of
    //     status codes in its APIs. This status comes from a user class and
    //     should not be returned directly unless it maps to a standardized
    //     code. For now, just remap FAILED_PRECONDITION because that value is
    //     used within the RPC system for another purpose (attempted to use a
    //     closed RPC call object). Long term, the statuses need to be
    //     standardized across all APIs. For example, this might return OK,
    //     UNAVAILABLE, or DATA_LOSS and other codes are mapped to UNKNOWN.
    if (sent.IsFailedPrecondition()) {
      sent = Status::Unknown();
    }
  }
  return sent;
}

}  // namespace pw::rpc::internal
