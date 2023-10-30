// Copyright 2023 The Pigweed Authors
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
#include <deque>
#include <vector>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_varint/varint.h"

namespace pw::containers {

// Behaves like a VariableLengthEntryQueue should, but with a std::deque-based
// implementation.
class VariableLengthEntryQueueTestOracle {
 public:
  VariableLengthEntryQueueTestOracle(uint32_t max_size_bytes)
      : max_size_bytes_(max_size_bytes),
        raw_size_bytes_(0),
        raw_capacity_bytes_(
            static_cast<uint32_t>(varint::EncodedSize(max_size_bytes)) +
            max_size_bytes) {}

  void clear() {
    q_.clear();
    raw_size_bytes_ = 0;
  }

  void push_overwrite(ConstByteSpan data) {
    size_t encoded_size = varint::EncodedSize(data.size()) + data.size();
    while (encoded_size > (raw_capacity_bytes_ - raw_size_bytes_)) {
      pop();
    }
    push(data);
  }

  void push(ConstByteSpan data) {
    PW_ASSERT(data.size() <= max_size_bytes_);

    size_t encoded_size = varint::EncodedSize(data.size()) + data.size();
    PW_ASSERT(encoded_size <= raw_capacity_bytes_ - raw_size_bytes_);

    q_.emplace_back(data.begin(), data.end());
    raw_size_bytes_ += encoded_size;
  }

  void pop() {
    PW_ASSERT(!q_.empty());
    raw_size_bytes_ -=
        varint::EncodedSize(q_.front().size()) + q_.front().size();
    q_.pop_front();
  }

  uint32_t size() const { return static_cast<uint32_t>(q_.size()); }
  uint32_t size_bytes() const {
    uint32_t total_bytes = 0;
    for (const auto& entry : q_) {
      total_bytes += static_cast<uint32_t>(entry.size());
    }
    return total_bytes;
  }
  uint32_t max_size_bytes() const { return max_size_bytes_; }

  std::deque<std::vector<std::byte>>::const_iterator begin() const {
    return q_.begin();
  }

  std::deque<std::vector<std::byte>>::const_iterator end() const {
    return q_.end();
  }

 private:
  std::deque<std::vector<std::byte>> q_;
  const uint32_t max_size_bytes_;
  uint32_t raw_size_bytes_;
  const uint32_t raw_capacity_bytes_;
};

}  // namespace pw::containers
