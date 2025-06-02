// Copyright 2025 The Pigweed Authors
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

#include <type_traits>
#include <utility>

namespace pw::containers::internal {

/// Mix-in for containers that hold up to a certain number of items.
///
/// Non-intrusive containers such as deques, queues, and vectors track both
/// their overall capacity and the number of items currently present. This
/// implementation of a type to manage those values does not add any additional
/// behaviors when those values change.
template <typename SizeType>
class CountAndCapacity {
 public:
  using size_type = SizeType;

  static_assert(std::is_unsigned_v<SizeType>, "SizeType must be unsigned.");

  constexpr explicit CountAndCapacity(size_type capacity)
      : capacity_(capacity) {}

  constexpr CountAndCapacity(CountAndCapacity&& other) {
    *this = std::move(other);
  }

  constexpr CountAndCapacity& operator=(CountAndCapacity&& other) noexcept {
    capacity_ = std::exchange(other.capacity_, 0);
    count_ = std::exchange(other.count_, 0);
    return *this;
  }

  constexpr size_type capacity() const noexcept { return capacity_; }
  constexpr size_type count() const noexcept { return count_; }

  constexpr void SetCount(size_type count) { count_ = count; }

  constexpr void IncrementCount(size_type n = 1) { count_ += n; }

  constexpr void DecrementCount(size_type n = 1) { count_ -= n; }

  void SetCapacity(size_type capacity) { capacity_ = capacity; }

 private:
  size_type capacity_;
  size_type count_ = 0;
};

}  // namespace pw::containers::internal
