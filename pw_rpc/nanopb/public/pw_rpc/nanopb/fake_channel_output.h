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
#include <cstdint>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_rpc/internal/fake_channel_output.h"
#include "pw_rpc/nanopb/internal/method.h"

namespace pw::rpc {

// A ChannelOutput implementation that stores the outgoing payloads and status.
template <typename Response, size_t kMaxResponses, size_t kOutputSize>
class NanopbFakeChannelOutput final
    : public internal::test::FakeChannelOutputBuffer<kOutputSize> {
 public:
  // Creates a NanopbFakeChannelOutput for the specified method.
  template <typename ServiceType, auto kMethod, uint32_t kMethodId>
  static NanopbFakeChannelOutput Create() {
    return NanopbFakeChannelOutput(
        internal::MethodTraits<decltype(kMethod)>::kType,
        internal::MethodLookup::GetNanopbMethod<ServiceType, kMethodId>());
  }

  // Private constructor, do not use. This constructor is exposed so this class
  // can be constructed using std::make_from_tuple in InvocationContext.
  NanopbFakeChannelOutput(MethodType method_type,
                          const internal::NanopbMethod& kMethod)
      : internal::test::FakeChannelOutputBuffer<kOutputSize>(method_type),
        method_(kMethod) {}

  const Vector<Response>& responses() const { return responses_; }

  const Response& last_response() const {
    PW_ASSERT(!responses_.empty());
    return responses_.back();
  }

  Response& AllocateResponse() {
    // If we run out of space, the back message is always the most recent.
    responses_.emplace_back();
    responses_.back() = {};
    return responses_.back();
  }

 private:
  void AppendResponse(ConstByteSpan response) override {
    Response& response_struct = AllocateResponse();
    PW_ASSERT(method_.serde().DecodeResponse(response, &response_struct));
  }

  void ClearResponses() override { responses_.clear(); }

  const internal::NanopbMethod& method_;
  Vector<Response, kMaxResponses> responses_;
};

}  // namespace pw::rpc
