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

#include <initializer_list>
#include <utility>

#include "pw_containers/internal/traits.h"

namespace pw::containers::internal {

// Generic array-based deque class. Uses CRTP to access the underlying deque and
// handle potentially resizing it.
//
// Extended by pw::InlineQueue and pw::DynamicQueue.
template <typename Derived, typename Deque>
class GenericQueue {
 public:
  using const_iterator = typename Deque::const_iterator;
  using const_pointer = typename Deque::const_pointer;
  using const_reference = typename Deque::const_reference;
  using difference_type = typename Deque::difference_type;
  using iterator = typename Deque::iterator;
  using pointer = typename Deque::pointer;
  using reference = typename Deque::reference;
  using size_type = typename Deque::size_type;
  using value_type = typename Deque::value_type;

  // Access

  reference front() { return deque().front(); }
  const_reference front() const { return deque().front(); }

  reference back() { return deque().back(); }
  const_reference back() const { return deque().back(); }

  // Size
  [[nodiscard]] bool empty() const noexcept { return deque().empty(); }

  size_type size() const noexcept { return deque().size(); }

  size_type max_size() const noexcept { return capacity(); }

  size_type capacity() const noexcept { return deque().capacity(); }

  // Modify

  void push(const value_type& value) { emplace(value); }

  void push(value_type&& value) { emplace(std::move(value)); }

  template <typename... Args>
  void emplace(Args&&... args) {
    deque().emplace_back(std::forward<Args>(args)...);
  }

  void pop() { deque().pop_front(); }

 protected:
  // Constructors
  constexpr GenericQueue() noexcept {}

  // `GenericQueue` may not be used with `unique_ptr` or `delete`.
  // `delete` could be supported using C++20's destroying delete.
  ~GenericQueue() = default;

  constexpr Deque& deque() { return static_cast<Derived&>(*this).deque(); }
  constexpr const Deque& deque() const {
    return static_cast<const Derived&>(*this).deque();
  }
};

}  // namespace pw::containers::internal
