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

#include <cstddef>
#include <iterator>
#include <type_traits>

#include "pw_assert/assert.h"

namespace pw::containers::internal {

template <typename Derived, typename ValueType, typename SizeType>
class GenericDeque;

// DequeIterator meets the named requirements for LegacyRandomAccessIterator.
template <typename Container>
class DequeIterator {
 public:
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using reference =
      typename std::conditional_t<std::is_const_v<Container>,
                                  typename Container::const_reference,
                                  typename Container::reference>;
  using pointer = typename std::conditional_t<std::is_const_v<Container>,
                                              typename Container::const_pointer,
                                              typename Container::pointer>;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::forward_iterator_tag;

  constexpr DequeIterator() = default;

  constexpr DequeIterator(const DequeIterator& other) = default;
  constexpr DequeIterator& operator=(const DequeIterator& other) = default;

  // Support converting non-const iterators to const_iterators.
  constexpr operator DequeIterator<std::add_const_t<Container>>() const {
    return {container_, pos_};
  }

  constexpr DequeIterator& operator+=(difference_type n) { return Incr(n); }
  constexpr DequeIterator& operator-=(difference_type n) { return Incr(-n); }
  constexpr DequeIterator& operator++() { return Incr(1); }
  constexpr DequeIterator operator++(int) {
    DequeIterator it(*this);
    operator++();
    return it;
  }

  constexpr DequeIterator& operator--() { return Incr(-1); }
  constexpr DequeIterator operator--(int) {
    DequeIterator it = *this;
    operator--();
    return it;
  }

  constexpr friend DequeIterator operator+(DequeIterator it,
                                           difference_type n) {
    return it += n;
  }
  constexpr friend DequeIterator operator+(difference_type n,
                                           DequeIterator it) {
    return it += n;
  }

  constexpr friend DequeIterator operator-(DequeIterator it,
                                           difference_type n) {
    return it -= n;
  }

  constexpr friend difference_type operator-(const DequeIterator& a,
                                             const DequeIterator& b) {
    return static_cast<difference_type>(a.pos_ == kEnd ? a.container_->size()
                                                       : a.pos_) -
           static_cast<difference_type>(b.pos_ == kEnd ? b.container_->size()
                                                       : b.pos_);
  }

  constexpr reference operator*() const {
    PW_DASSERT(pos_ != kEnd);
    PW_DASSERT(pos_ < container_->size());

    return container_->at(pos_);
  }

  constexpr pointer operator->() const {
    PW_DASSERT(pos_ != kEnd);
    PW_DASSERT(pos_ < container_->size());

    return &**this;
  }

  constexpr reference operator[](size_type n) { return *(*this + n); }

  constexpr friend bool operator==(DequeIterator a, DequeIterator b) {
    return a.container_ == b.container_ && a.pos_ == b.pos_;
  }

  constexpr friend bool operator!=(DequeIterator a, DequeIterator b) {
    return !(a == b);
  }

  constexpr friend bool operator<(DequeIterator a, DequeIterator b) {
    return b - a > 0;
  }

  constexpr friend bool operator>(DequeIterator a, DequeIterator b) {
    return b < a;
  }

  constexpr friend bool operator<=(DequeIterator a, DequeIterator b) {
    return !(a > b);
  }

  constexpr friend bool operator>=(DequeIterator a, DequeIterator b) {
    return !(a < b);
  }

 private:
  template <typename Derived, typename ValueType, typename SizeType>
  friend class GenericDeque;

  // Allow non-const iterators to construct const_iterators in conversions.
  friend class DequeIterator<std::remove_const_t<Container>>;

  constexpr DequeIterator(Container* container, size_type pos)
      : container_(container), pos_(pos) {}

  constexpr DequeIterator& Incr(difference_type n) {
    difference_type difference = n + (pos_ == kEnd ? container_->size() : pos_);
    PW_DASSERT(difference >= 0);

    size_type new_pos = static_cast<size_type>(difference);
    PW_DASSERT(new_pos <= container_->size());
    pos_ = new_pos == container_->size() ? kEnd : new_pos;
    return *this;
  }

  static constexpr size_type kEnd = std::numeric_limits<size_type>::max();

  Container* container_;  // pointer to container this iterator is from
  size_type pos_;         // logical index of iterator
};

}  // namespace pw::containers::internal
