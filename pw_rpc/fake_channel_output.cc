// Copyright 2021 The Pigweed Authors
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

#include "pw_rpc_private/fake_channel_output.h"

#include "pw_assert/check.h"
#include "pw_result/result.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc::internal::test {

void FakeChannelOutput::clear() {
  ClearResponses();
  total_responses_ = 0;
  last_status_ = Status::Unknown();
  done_ = false;
}

Status FakeChannelOutput::SendAndReleaseBuffer(
    std::span<const std::byte> buffer) {
  PW_CHECK_PTR_EQ(buffer.data(), packet_buffer_.data());

  // If the buffer is empty, this is just releasing an unused buffer.
  if (buffer.empty()) {
    return OkStatus();
  }

  PW_CHECK(!done_);

  Result<Packet> result = Packet::FromBuffer(buffer);
  PW_CHECK_OK(result.status());

  last_status_ = result.value().status();

  switch (result.value().type()) {
    case PacketType::RESPONSE:
      // Server streaming RPCs don't have a payload in their response packet.
      if (!server_streaming_) {
        ProcessResponse(result.value().payload());
      }
      done_ = true;
      break;
    case PacketType::SERVER_STREAM:
      ProcessResponse(result.value().payload());
      break;
    default:
      PW_CRASH("Unhandled PacketType %d",
               static_cast<int>(result.value().type()));
  }
  return OkStatus();
}

}  // namespace pw::rpc::internal::test
