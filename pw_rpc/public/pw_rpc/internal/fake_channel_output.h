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

  size_t total_responses() const { return total_responses_; }

  // Set to true if a RESPONSE packet is seen.
  bool done() const { return done_; }

  void clear();

 protected:
  constexpr FakeChannelOutput(MethodType method_type, ByteSpan packet_buffer)
      : ChannelOutput("pw::rpc::internal::test::FakeChannelOutput"),
        packet_buffer_(packet_buffer),
        method_type_(method_type) {}

 private:
  ByteSpan AcquireBuffer() final { return packet_buffer_; }

  Status SendAndReleaseBuffer(ConstByteSpan buffer) final;

  virtual void AppendResponse(ConstByteSpan response) = 0;
  virtual void ClearResponses() = 0;

  void ProcessResponse(ConstByteSpan response) {
    AppendResponse(response);
    total_responses_ += 1;
  }

  const ByteSpan packet_buffer_;
  size_t total_responses_ = 0;
  Status last_status_;
  bool done_ = false;
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
