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
#pragma once

#include <cstddef>

#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::kvs {

class ChecksumAlgorithm {
 public:
  // Resets the checksum to its initial state.
  virtual void Reset() = 0;

  // Updates the checksum with the provided data.
  virtual void Update(span<const std::byte> data) = 0;

  // Update the checksum from a pointer and size.
  void Update(const void* data, size_t size_bytes) {
    return Update(span(static_cast<const std::byte*>(data), size_bytes));
  }

  // Returns the current state of the checksum algorithm.
  constexpr const span<const std::byte>& state() const { return state_; }

  // Returns the size of the checksum state.
  constexpr size_t size_bytes() const { return state_.size(); }

  // Compares a calculated checksum to this checksum's current state.
  Status Verify(span<const std::byte> checksum) const;

 protected:
  // A derived class provides a span of its state buffer.
  constexpr ChecksumAlgorithm(span<const std::byte> state) : state_(state) {}

  // Protected destructor prevents deleting ChecksumAlgorithms from the base
  // class, so that it is safe to have a non-virtual destructor.
  ~ChecksumAlgorithm() = default;

 private:
  span<const std::byte> state_;
};

}  // namespace pw::kvs
