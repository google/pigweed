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
#include "pw_containers/wrapped_iterator.h"
#include "pw_rpc/internal/fake_channel_output.h"
#include "pw_rpc/nanopb/internal/common.h"
#include "pw_rpc/nanopb/internal/method.h"

namespace pw::rpc {

// Supports iterating over payloads as decoded Nanopb structs.
template <typename Payload>
class NanopbPayloadsView {
 public:
  class iterator : public containers::WrappedIterator<iterator,
                                                      PayloadsView::iterator,
                                                      Payload> {
   public:
    // Access the payload (rather than packet) with operator*.
    Payload operator*() const {
      Payload payload{};
      PW_ASSERT(serde_.Decode(Base::value(), &payload));
      return payload;
    }

   private:
    friend class NanopbPayloadsView;

    constexpr iterator(const PayloadsView::iterator& it,
                       const internal::NanopbSerde& serde)
        : Base(it), serde_(serde) {}

    internal::NanopbSerde serde_;
  };

  Payload operator[](size_t index) const {
    Payload payload{};
    PW_ASSERT(serde_.Decode(view_[index], &payload));
    return payload;
  }

  size_t size() const { return view_.size(); }
  bool empty() const { return view_.empty(); }

  // Returns the first/last payload for the RPC. size() must be > 0.
  Payload front() const { return *begin(); }
  Payload back() const { return *std::prev(end()); }

  iterator begin() const { return iterator(view_.begin(), serde_); }
  iterator end() const { return iterator(view_.end(), serde_); }

 private:
  using Base =
      containers::WrappedIterator<iterator, PayloadsView::iterator, Payload>;

  template <size_t, size_t, size_t>
  friend class NanopbFakeChannelOutput;

  template <typename... Args>
  NanopbPayloadsView(const internal::NanopbSerde& serde, Args&&... args)
      : view_(args...), serde_(serde) {}

  PayloadsView view_;
  internal::NanopbSerde serde_;
};

// A ChannelOutput implementation that stores the outgoing payloads and status.
template <size_t kMaxPackets,
          size_t kOutputSize,
          size_t kPayloadsBufferSizeBytes = 128>
class NanopbFakeChannelOutput final
    : public internal::test::FakeChannelOutputBuffer<kOutputSize,
                                                     kMaxPackets,
                                                     kPayloadsBufferSizeBytes> {
 private:
  template <auto kMethod>
  using Response = typename internal::MethodInfo<kMethod>::Response;

 public:
  NanopbFakeChannelOutput() = default;

  template <auto kMethod>
  NanopbPayloadsView<Response<kMethod>> responses(
      uint32_t channel_id = Channel::kUnassignedChannelId) const {
    return NanopbPayloadsView<Response<kMethod>>(
        internal::MethodInfo<kMethod>::serde().response(),
        Base::packets(),
        internal::MethodInfo<kMethod>::kType,
        channel_id,
        internal::MethodInfo<kMethod>::kServiceId,
        internal::MethodInfo<kMethod>::kMethodId);
  }

  template <typename ResponseType>
  NanopbPayloadsView<ResponseType> responses(const internal::NanopbSerde& serde,
                                             MethodType type,
                                             uint32_t channel_id,
                                             uint32_t service_id,
                                             uint32_t method_id) const {
    return NanopbPayloadsView<ResponseType>(
        serde, Base::packets(), type, channel_id, service_id, method_id);
  }

  template <auto kMethod>
  Response<kMethod> last_response() const {
    NanopbPayloadsView<Response<kMethod>> payloads = responses<kMethod>();
    PW_ASSERT(!payloads.empty());
    return payloads.back();
  }

 private:
  using Base =
      internal::test::FakeChannelOutputBuffer<kOutputSize,
                                              kMaxPackets,
                                              kPayloadsBufferSizeBytes>;
};

}  // namespace pw::rpc
