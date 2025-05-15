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

#include <utility>

#include "pw_allocator/allocator.h"
#include "pw_containers/dynamic_deque.h"

namespace pw {

/// A queue implementation backed by `pw::DynamicDeque`.
///
/// This class provides a `std::queue`-like interface but uses a Pigweed
/// allocator for dynamic memory management. It includes fallible `try_*`
/// operations for scenarios where allocation failure may be handled gracefully.
///
/// @tparam T The type of elements stored in the queue.
template <typename T, typename SizeType = uint16_t>
class DynamicQueue {
 public:
  using container_type = DynamicDeque<T, SizeType>;
  using value_type = T;
  using size_type = typename container_type::size_type;
  using reference = value_type&;
  using const_reference = const value_type&;

  /// Constructs a `DynamicQueue` using the provided allocator.
  explicit constexpr DynamicQueue(pw::Allocator& allocator)
      : deque_(allocator) {}

  // Disable copy since it can fail.
  DynamicQueue(const DynamicQueue&) = delete;
  DynamicQueue& operator=(const DynamicQueue&) = delete;

  /// Move operations are supported and incur no allocations.
  constexpr DynamicQueue(DynamicQueue&&) = default;
  DynamicQueue& operator=(DynamicQueue&&) = default;

  /// Checks if the queue is empty.
  [[nodiscard]] bool empty() const { return deque_.empty(); }

  /// Returns the number of elements in the queue.
  size_type size() const { return deque_.size(); }

  /// Maximum possible queue `size()`, ignoring limitations of the allocator.
  size_type max_size() const { return deque_.max_size(); }

  /// Returns the number of elements the queue can hold without attempting to
  /// allocate.
  size_type capacity() const { return deque_.capacity(); }

  /// Returns a reference to the first element in the queue.
  reference front() { return deque_.front(); }

  /// Returns a const reference to the first element in the queue.
  const_reference front() const { return deque_.front(); }

  /// Returns a reference to the last element in the queue.
  reference back() { return deque_.back(); }

  /// Returns a const reference to the last element in the queue.
  const_reference back() const { return deque_.back(); }

  /// Removes all elements from the queue.
  void clear() { deque_.clear(); }

  /// Copies an element to the back of the queue. Crashes on allocation failure.
  void push(const value_type& value) { deque_.push_back(value); }

  /// Moves an element to the back of the queue. Crashes on allocation failure.
  void push(value_type&& value) { deque_.push_back(std::move(value)); }

  /// Constructs an element in place at the back of the queue. Crashes on
  /// allocation failure.
  template <typename... Args>
  void emplace(Args&&... args) {
    deque_.emplace_back(std::forward<Args>(args)...);
  }

  /// Removes the first element from the queue. The queue must not be empty.
  void pop() { deque_.pop_front(); }

  /// Attempts to add an element to the back of the queue.
  [[nodiscard]] bool try_push(const value_type& value) {
    return deque_.try_push_back(value);
  }

  /// Attempts to add an element to the back of the queue (move version).
  [[nodiscard]] bool try_push(value_type&& value) {
    return deque_.try_push_back(std::move(value));
  }

  /// Attempts to construct an element in place at the back of the queue.
  template <typename... Args>
  [[nodiscard]] bool try_emplace(Args&&... args) {
    return deque_.try_emplace_back(std::forward<Args>(args)...);
  }

  /// Sets the queue capacity to `max(capacity, size())` elements.
  void reserve(size_type capacity) { deque_.reserve(capacity); }

  /// Attempts to set the queue capacity to `max(capacity, size())` elements.
  [[nodiscard]] bool try_reserve(size_type capacity) {
    return deque_.try_reserve(capacity);
  }

  /// Reduces memory usage by releasing unused capacity, if possible.
  void shrink_to_fit() { deque_.shrink_to_fit(); }

  /// Swaps the contents with another queue.
  void swap(DynamicQueue& other) { deque_.swap(other.deque_); }

 private:
  container_type deque_;
};

}  // namespace pw
