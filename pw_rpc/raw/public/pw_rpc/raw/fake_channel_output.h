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

#include <array>
#include <cstddef>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_rpc/internal/fake_channel_output.h"
#include "pw_rpc/raw/internal/method.h"

namespace pw::rpc {

// A ChannelOutput implementation that stores the outgoing payloads and status.
template <size_t kOutputSize, size_t kMaxResponses>
class RawFakeChannelOutput final : public internal::test::FakeChannelOutput {
 public:
  RawFakeChannelOutput(MethodType method_type)
      : FakeChannelOutput(packet_buffer_, method_type) {}

  const Vector<ByteSpan>& responses() const { return responses_; }

  ConstByteSpan last_response() const {
    PW_ASSERT(!responses_.empty());
    return responses_.back();
  }

  // Allocates a response buffer and returns a reference to the response span
  // for it.
  ByteSpan& AllocateResponse() {
    // If we run out of space, the back message is always the most recent.
    response_buffers_.emplace_back();
    response_buffers_.back() = {};

    responses_.emplace_back();
    responses_.back() = {response_buffers_.back().data(),
                         response_buffers_.back().size()};
    return responses_.back();
  }

 private:
  void AppendResponse(ConstByteSpan response) override {
    ByteSpan& response_span = AllocateResponse();
    PW_ASSERT(response.size() <= response_span.size());

    std::memcpy(response_span.data(), response.data(), response.size());
    response_span = response_span.first(response.size());
  }

  void ClearResponses() override {
    responses_.clear();
    response_buffers_.clear();
  }

  std::array<std::byte, kOutputSize> packet_buffer_;
  Vector<ByteSpan, kMaxResponses> responses_;
  Vector<std::array<std::byte, kOutputSize>, kMaxResponses> response_buffers_;
};

}  // namespace pw::rpc
