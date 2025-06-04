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

#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/waker.h"
#include "pw_containers/internal/count_and_capacity.h"

namespace pw::containers::internal {

/// Mix-in for containers that hold up to a certain number of items.
///
/// Non-intrusive containers such as deques, queues, and vectors track both
/// their overall capacity and the number of items currently present. This type
/// extends the basic `CountAndCapacity` functionality to add the ability to
/// wake a task that is pending on enough space becoming available.
template <typename SizeType>
class AsyncCountAndCapacity {
 public:
  using size_type = SizeType;

  constexpr explicit AsyncCountAndCapacity(size_type capacity)
      : count_and_capacity_(capacity) {}
  constexpr AsyncCountAndCapacity(AsyncCountAndCapacity&& other) {
    *this = std::move(other);
  }
  constexpr AsyncCountAndCapacity& operator=(
      AsyncCountAndCapacity&& other) noexcept {
    count_and_capacity_ = std::move(other.count_and_capacity_);
    requested_ = std::exchange(other.requested_, 0);
    waker_ = std::move(other.waker_);
    return *this;
  }

  constexpr size_type capacity() const noexcept {
    return count_and_capacity_.capacity();
  }
  constexpr size_type count() const noexcept {
    return count_and_capacity_.count();
  }

  constexpr void SetCount(size_type count) {
    count_and_capacity_.SetCount(count);
    CheckRequested();
  }

  constexpr void IncrementCount(size_type n = 1) {
    count_and_capacity_.IncrementCount(n);
  }

  constexpr void DecrementCount(size_type n = 1) {
    count_and_capacity_.DecrementCount(n);
    CheckRequested();
  }

  /// Waits until enough room is available in the container.
  ///
  /// The `num` of requested elements must be less than the capacity.
  async2::Poll<> PendAvailable(async2::Context& context, size_type num) {
    PW_ASSERT(num <= capacity());
    if (num <= capacity() - count()) {
      return async2::Ready();
    }
    PW_ASYNC_STORE_WAKER(
        context, waker_, "waiting for space for items in container");
    requested_ = num;
    return async2::Pending();
  }

  constexpr void SetCapacity(size_type capacity) {
    count_and_capacity_.SetCapacity(capacity);
    CheckRequested();
  }

 private:
  void CheckRequested() {
    if (requested_ == 0 || capacity() - count() < requested_) {
      return;
    }
    requested_ = 0;
    std::move(waker_).Wake();
  }

  CountAndCapacity<size_type> count_and_capacity_;
  size_type requested_ = 0;
  async2::Waker waker_;
};

}  // namespace pw::containers::internal
