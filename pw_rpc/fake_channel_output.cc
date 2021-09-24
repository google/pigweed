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

#include "pw_rpc/internal/fake_channel_output.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc::internal::test {

void FakeChannelOutput::clear() {
  payloads_.clear();
  packets_.clear();
  total_response_packets_ = 0;
  total_stream_packets_ = 0;
  send_status_ = OkStatus();
  return_after_packet_count_ = -1;
}

Status FakeChannelOutput::SendAndReleaseBuffer(
    std::span<const std::byte> buffer) {
  PW_CHECK_PTR_EQ(buffer.data(), encoding_buffer_.data());

  // If the buffer is empty, this is just releasing an unused buffer.
  if (buffer.empty()) {
    return OkStatus();
  }

  if (return_after_packet_count_ == 0) {
    return send_status_;
  }
  if (return_after_packet_count_ > 0 &&
      return_after_packet_count_ == static_cast<int>(total_responses())) {
    // Disable behavior.
    return_after_packet_count_ = -1;
    return send_status_;
  }

  Result<Packet> result = Packet::FromBuffer(buffer);
  PW_CHECK_OK(result.status());

  PW_CHECK(!packets_.full(),
           "Attempted to store more than %u packets. Increase the kMaxPackets "
           "template arg to store more packets.",
           static_cast<unsigned>(packets_.size()));

  packets_.push_back(*result);

  CopyPayloadToBuffer(packets_.back().payload());

  switch (result.value().type()) {
    case PacketType::RESPONSE:
      ++total_response_packets_;
      break;
    case PacketType::SERVER_ERROR:
      PW_CRASH("Server error: %s", result.value().status().str());
    case PacketType::SERVER_STREAM:
      ++total_stream_packets_;
      break;
    default:
      PW_CRASH("Unhandled PacketType %d",
               static_cast<int>(result.value().type()));
  }
  return OkStatus();
}

void FakeChannelOutput::CopyPayloadToBuffer(const ConstByteSpan& payload) {
  if (payload.empty()) {
    return;
  }

  const size_t available_bytes = payloads_.max_size() - payloads_.size();
  PW_CHECK_UINT_GE(available_bytes,
                   payload.size(),
                   "Ran out of payload buffer space. Increase "
                   "kPayloadBufferSizeBytes (%u) or use smaller payloads.",
                   static_cast<unsigned>(payloads_.max_size()));

  const size_t start = payloads_.size();
  payloads_.resize(payloads_.size() + payload.size());
  std::memcpy(&payloads_[start], payload.data(), payload.size());
  packets_.back().set_payload(std::span(&payloads_[start], payload.size()));
}

}  // namespace pw::rpc::internal::test
