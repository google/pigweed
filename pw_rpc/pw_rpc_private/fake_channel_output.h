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

#include "pw_bytes/span.h"
#include "pw_rpc/channel.h"

namespace pw::rpc::internal::test {

// A ChannelOutput implementation that stores the outgoing payloads and status.
class FakeChannelOutput : public ChannelOutput {
 public:
  constexpr FakeChannelOutput(ByteSpan buffer, bool server_streaming)
      : ChannelOutput("pw::rpc::internal::test::FakeChannelOutput"),
        packet_buffer_(buffer),
        server_streaming_(server_streaming) {}

  Status last_status() const { return last_status_; }
  void set_last_status(Status status) { last_status_ = status; }

  size_t total_responses() const { return total_responses_; }

  // Set to true if a RESPONSE packet is seen.
  bool done() const { return done_; }

  void clear();

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
  const bool server_streaming_;
};

}  // namespace pw::rpc::internal::test
