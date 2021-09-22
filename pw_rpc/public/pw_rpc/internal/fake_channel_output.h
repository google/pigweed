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
#pragma once

#include <cstddef>

#include "pw_bytes/span.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/method_type.h"

namespace pw::rpc::internal::test {

// A ChannelOutput implementation that stores the outgoing payloads and status.
class FakeChannelOutput : public ChannelOutput {
 public:
  FakeChannelOutput(const FakeChannelOutput&) = delete;
  FakeChannelOutput(FakeChannelOutput&&) = delete;

  FakeChannelOutput& operator=(const FakeChannelOutput&) = delete;
  FakeChannelOutput& operator=(FakeChannelOutput&&) = delete;

  Status last_status() const {
    PW_ASSERT(done());
    return last_status_;
  }

  // Tracks the count for all the responses.
  size_t total_responses() const {
    return total_response_packets_ + total_stream_packets_;
  }
  // Track individual packet type counts.
  size_t total_response_packets() const { return total_response_packets_; }
  size_t total_stream_packets() const { return total_stream_packets_; }

  // Set to true if a RESPONSE packet is seen.
  bool done() const { return total_response_packets_ > 0; }

  void clear();

  // Returns `status` for all future SendAndReleaseBuffer calls. Enables packet
  // processing if `status` is OK.
  void set_send_status(Status status) {
    send_status_ = status;
    return_after_packet_count_ = status.ok() ? -1 : 0;
  }
  // Returns `status` once after the specified positive number of packets.
  void set_send_status(Status status, int return_after_packet_count) {
    PW_ASSERT(!status.ok());
    PW_ASSERT(return_after_packet_count > 0);
    send_status_ = status;
    return_after_packet_count_ = return_after_packet_count;
  }

 protected:
  constexpr FakeChannelOutput(MethodType method_type, ByteSpan packet_buffer)
      : ChannelOutput("pw::rpc::internal::test::FakeChannelOutput"),
        packet_buffer_(packet_buffer),
        method_type_(method_type) {}

 private:
  ByteSpan AcquireBuffer() final { return packet_buffer_; }

  // Processes buffer according to packet type and `return_after_packet_count_`
  // value as follows:
  // When positive, returns `send_status_` once,
  // When equals 0, returns `send_status_` in all future calls,
  // When negative, ignores `send_status_` processes buffer.
  Status SendAndReleaseBuffer(ConstByteSpan buffer) final;

  virtual void AppendResponse(ConstByteSpan response) = 0;
  virtual void ClearResponses() = 0;

  void ProcessResponse(ConstByteSpan response) { AppendResponse(response); }

  size_t total_response_packets_ = 0;
  size_t total_stream_packets_ = 0;

  int return_after_packet_count_ = -1;

  Status send_status_ = OkStatus();
  const ByteSpan packet_buffer_;
  Status last_status_;
  const MethodType method_type_;
};

// Adds the packet output buffer to a FakeChannelOutput.
template <size_t kOutputSizeBytes>
class FakeChannelOutputBuffer : public FakeChannelOutput {
 protected:
  constexpr FakeChannelOutputBuffer(MethodType method_type)
      : FakeChannelOutput(method_type, packet_bytes), packet_bytes{} {}

  std::byte packet_bytes[kOutputSizeBytes];
};

}  // namespace pw::rpc::internal::test
