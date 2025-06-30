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

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_containers/internal/deque_iterator.h"
#include "pw_containers/internal/traits.h"
#include "pw_span/span.h"

namespace pw::containers::internal {

template <typename T>
using EnableIfIterable =
    std::enable_if_t<true, decltype(T().begin(), T().end())>;

/// Base class for deques.
///
/// This type does not depend on the type of the elements being stored in the
/// container.
///
/// @tparam  CountAndCapacityType  Type that encapsulates the containers overall
///                                capacity and current count of elements.
///                                Different implementation may add additional
///                                behavior when the count or capacity changes.
template <typename CountAndCapacityType>
class GenericDequeBase {
 public:
  using size_type = typename CountAndCapacityType::size_type;

  // Size

  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  /// Returns the number of elements in the deque.
  constexpr size_type size() const noexcept {
    return count_and_capacity_.count();
  }

  /// Returns the maximum number of elements in the deque.
  constexpr size_type capacity() const noexcept {
    return count_and_capacity_.capacity();
  }

 protected:
  // Functions used by deque implementations

  CountAndCapacityType& count_and_capacity() noexcept {
    return count_and_capacity_;
  }

  constexpr void MoveAssignIndices(GenericDequeBase& other) noexcept {
    count_and_capacity_ = std::move(other.count_and_capacity_);
    head_ = std::exchange(other.head_, 0);
    tail_ = std::exchange(other.tail_, 0);
  }

  void SwapIndices(GenericDequeBase& other) noexcept {
    std::swap(count_and_capacity_, other.count_and_capacity_);
    std::swap(head_, other.head_);
    std::swap(tail_, other.tail_);
  }

  // Returns if buffer can be resized larger without moving any items. Can
  // extend if not wrapped or if tail_ wrapped but no elements were added.
  bool CanExtendBuffer() const { return tail_ > head_ || tail_ == 0; }

  // Can only shrink if there are no empty slots at the start of the buffer.
  bool CanShrinkBuffer() const { return head_ == 0; }

  void HandleNewBuffer(size_type new_capacity) {
    count_and_capacity_.SetCapacity(new_capacity);
    head_ = 0;
    tail_ = size() == new_capacity
                ? 0
                : count_and_capacity_.count();  // handle full buffers
  }

  void HandleExtendedBuffer(size_type new_capacity) {
    count_and_capacity_.SetCapacity(new_capacity);
    if (tail_ == 0) {  // "unwrap" the tail if needed
      tail_ = head_ + count_and_capacity_.count();
    }
  }

  void HandleShrunkBuffer(size_type new_capacity) {
    count_and_capacity_.SetCapacity(new_capacity);
    if (tail_ == new_capacity) {  // wrap the tail if needed
      tail_ = 0;
    }
  }

 private:
  // Functions needed by GenericDeque only
  template <typename Derived, typename ValueType, typename S>
  friend class GenericDeque;

  explicit constexpr GenericDequeBase(size_type initial_capacity) noexcept
      : count_and_capacity_(initial_capacity), head_(0), tail_(0) {}

  constexpr void ClearIndices() {
    count_and_capacity_.SetCount(0);
    head_ = tail_ = 0;
  }

  // Returns the absolute index based on the relative index beyond the
  // head offset.
  //
  // Precondition: The relative index must be valid, i.e. < size().
  constexpr size_type AbsoluteIndex(const size_type relative_index) const {
    const size_type absolute_index = head_ + relative_index;
    if (absolute_index < capacity()) {
      return absolute_index;
    }
    // Offset wrapped across the end of the circular buffer.
    return absolute_index - capacity();
  }

  constexpr size_type AbsoluteIndexChecked(
      const size_type relative_index) const {
    PW_ASSERT(relative_index < size());
    return AbsoluteIndex(relative_index);
  }

  constexpr void PushBack() {
    IncrementWithWrap(tail_);
    count_and_capacity_.IncrementCount();
  }
  constexpr void PushFront() {
    DecrementWithWrap(head_);
    count_and_capacity_.IncrementCount();
  }
  constexpr void PopFront() {
    IncrementWithWrap(head_);
    count_and_capacity_.DecrementCount();
  }
  constexpr void PopBack() {
    DecrementWithWrap(tail_);
    count_and_capacity_.DecrementCount();
  }

  constexpr void IncrementWithWrap(size_type& index) const {
    index++;
    // Note: branch is faster than mod (%) on common embedded architectures.
    if (index == capacity()) {
      index = 0;
    }
  }

  constexpr void DecrementWithWrap(size_type& index) const {
    if (index == 0) {
      index = capacity();
    }
    index--;
  }

  CountAndCapacityType count_and_capacity_;
  size_type head_;  // Inclusive offset for the front.
  size_type tail_;  // Non-inclusive offset for the back.
};

/// Generic array-based deque class.
///
/// Uses CRTP to access the underlying array and handle potentially resizing it.
/// Extended by pw::InlineDeque and pw::DynamicDeque.
template <typename Derived, typename ValueType, typename CountAndCapacityType>
class GenericDeque : public GenericDequeBase<CountAndCapacityType> {
 private:
  using Base = GenericDequeBase<CountAndCapacityType>;

 public:
  using value_type = ValueType;
  using size_type = typename CountAndCapacityType::size_type;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = containers::internal::DequeIterator<Derived>;
  using const_iterator = containers::internal::DequeIterator<const Derived>;

  // Copy/assign are implemented in derived classes.
  GenericDeque(const GenericDeque&) = delete;
  GenericDeque(GenericDeque&&) = delete;

  GenericDeque& operator=(const GenericDeque&) = delete;
  GenericDeque&& operator=(GenericDeque&&) = delete;

  // Size

  using Base::capacity;
  using Base::empty;
  using Base::size;

  // Infallible assign

  /// Sets the contents to `count` copies of `value`. Crashes if cannot fit.
  void assign(size_type count, const value_type& value) {
    PW_ASSERT(try_assign(count, value));
  }

  /// Sets the contents to copies of the items from the iterator. Crashes if
  /// cannot fit.
  template <typename It,
            int&...,
            typename = containers::internal::EnableIfInputIterator<It>>
  void assign(It start, It finish);

  /// Sets contents to copies of the items from the list. Crashes if cannot fit.
  void assign(const std::initializer_list<value_type>& list) {
    assign(list.begin(), list.end());
  }

  // Access

  constexpr reference at(size_type index) {
    return data()[Base::AbsoluteIndexChecked(index)];
  }
  constexpr const_reference at(size_type index) const {
    return data()[Base::AbsoluteIndexChecked(index)];
  }

  constexpr reference operator[](size_type index) {
    PW_DASSERT(index < size());
    return data()[Base::AbsoluteIndex(index)];
  }
  constexpr const_reference operator[](size_type index) const {
    PW_DASSERT(index < size());
    return data()[Base::AbsoluteIndex(index)];
  }

  constexpr reference front() {
    PW_DASSERT(!empty());
    return data()[head()];
  }
  constexpr const_reference front() const {
    PW_DASSERT(!empty());
    return data()[head()];
  }

  constexpr reference back() {
    PW_DASSERT(!empty());
    return data()[Base::AbsoluteIndex(size() - 1)];
  }
  constexpr const_reference back() const {
    PW_DASSERT(!empty());
    return data()[Base::AbsoluteIndex(size() - 1)];
  }

  /// Provides access to the valid data in a contiguous form.
  constexpr std::pair<span<const value_type>, span<const value_type>>
  contiguous_data() const;
  constexpr std::pair<span<value_type>, span<value_type>> contiguous_data() {
    auto [first, second] = std::as_const(*this).contiguous_data();
    return {{const_cast<pointer>(first.data()), first.size()},
            {const_cast<pointer>(second.data()), second.size()}};
  }

  // Iterate

  constexpr iterator begin() noexcept {
    if (empty()) {
      return end();
    }

    return iterator(derived(), 0);
  }
  constexpr const_iterator begin() const noexcept { return cbegin(); }
  constexpr const_iterator cbegin() const noexcept {
    if (empty()) {
      return cend();
    }
    return const_iterator(derived(), 0);
  }

  constexpr iterator end() noexcept { return iterator(derived(), size()); }
  constexpr const_iterator end() const noexcept { return cend(); }
  constexpr const_iterator cend() const noexcept {
    return const_iterator(derived(), size());
  }

  // Infallible modify

  void clear() {
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      std::destroy(begin(), end());
    }
    Base::ClearIndices();
  }

  /// Erases the item at `pos`, which must be a dereferenceable iterator.
  iterator erase(const_iterator pos) {
    PW_DASSERT(pos.pos_ < size());
    return erase(pos, pos + 1);
  }

  /// Erases the items in the range `[first, last)`. Does nothing if `first ==
  /// last`.
  iterator erase(const_iterator first, const_iterator last);

  void push_back(const value_type& value) { PW_ASSERT(try_push_back(value)); }

  void push_back(value_type&& value) {
    PW_ASSERT(try_push_back(std::move(value)));
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    PW_ASSERT(try_emplace_back(std::forward<Args>(args)...));
  }

  void pop_back();

  void push_front(const value_type& value) { PW_ASSERT(try_push_front(value)); }

  void push_front(value_type&& value) {
    PW_ASSERT(try_push_front(std::move(value)));
  }

  template <typename... Args>
  void emplace_front(Args&&... args) {
    PW_ASSERT(try_emplace_front(std::forward<Args>(args)...));
  }

  void pop_front();

  void resize(size_type new_size) { resize(new_size, value_type()); }

  void resize(size_type new_size, const value_type& value) {
    PW_ASSERT(try_resize(new_size, value));
  }

 protected:
  explicit constexpr GenericDeque(size_type initial_capacity) noexcept
      : GenericDequeBase<CountAndCapacityType>(initial_capacity) {}

  // Infallible assignment operators

  // NOLINTBEGIN(misc-unconventional-assign-operator);
  Derived& operator=(const std::initializer_list<value_type>& list) {
    assign(list);
    return derived();
  }

  template <typename T, typename = containers::internal::EnableIfIterable<T>>
  Derived& operator=(const T& other) {
    assign(other.begin(), other.end());
    return derived();
  }
  // NOLINTEND(misc-unconventional-assign-operator);

  // Fallible assign

  /// Attempts to replace the contents with `count` copies of `value`. If `count
  /// > capacity()` and allocation is supported, attempts to allocate to
  /// increase capacity. Does nothing if unable to accommodate `count` items.
  [[nodiscard]] bool try_assign(size_type count, const value_type& value);

  /// Replaces the container with copies of items from an iterator. Does nothing
  /// if unable to accommodate all items.
  ///
  /// `try_assign()` requires a forward iterator so that the capacity can be
  /// checked upfront to avoid partial assignments. Input iterators are only
  /// suitable for one pass, so could be exhausted by a `std::distance` check.
  template <typename It,
            int&...,
            typename = containers::internal::EnableIfForwardIterator<It>>
  [[nodiscard]] bool try_assign(It start, It finish);

  /// Replaces the container with copies of items from an initializer list. Does
  /// nothing if unable to accommodate all items.
  [[nodiscard]] bool try_assign(const std::initializer_list<value_type>& list) {
    return try_assign(list.begin(), list.end());
  }

  // Fallible modify

  [[nodiscard]] bool try_push_back(const value_type& value) {
    return try_emplace_back(value);
  }

  [[nodiscard]] bool try_push_back(value_type&& value) {
    return try_emplace_back(std::move(value));
  }

  template <typename... Args>
  [[nodiscard]] bool try_emplace_back(Args&&... args);

  [[nodiscard]] bool try_push_front(const value_type& value) {
    return try_emplace_front(value);
  }

  [[nodiscard]] bool try_push_front(value_type&& value) {
    return try_emplace_front(std::move(value));
  }

  template <typename... Args>
  [[nodiscard]] bool try_emplace_front(Args&&... args);

  [[nodiscard]] bool try_resize(size_type new_size) {
    return try_resize(new_size, value_type());
  }

  [[nodiscard]] bool try_resize(size_type new_size, const value_type& value);

 private:
  constexpr Derived& derived() { return static_cast<Derived&>(*this); }
  constexpr const Derived& derived() const {
    return static_cast<const Derived&>(*this);
  }

  constexpr size_type head() const { return Base::head_; }
  constexpr size_type tail() const { return Base::tail_; }

  // Accessed the underlying array in the derived class.
  constexpr pointer data() { return derived().data(); }
  constexpr const_pointer data() const { return derived().data(); }

  // Make sure the container can hold one more item.
  constexpr bool CheckCapacityAddOne() {
    return size() != std::numeric_limits<size_type>::max() &&
           CheckCapacity(size() + 1);
  }

  // Ensures that the container can hold at least this many elements.
  constexpr bool CheckCapacity(size_type new_size) {
    if constexpr (Derived::kFixedCapacity) {
      return new_size <= capacity();
    } else {
      return derived().try_reserve(new_size);
    }
  }

  // Appends items without checking the capacity. Capacity MUST be large enough.
  template <typename... Args>
  void EmplaceBackUnchecked(Args&&... args) {
    new (&data()[tail()]) value_type(std::forward<Args>(args)...);
    Base::PushBack();
  }
};

// Function implementations

template <typename Derived, typename ValueType, typename CountAndCapacityType>
template <typename It, int&..., typename>
void GenericDeque<Derived, ValueType, CountAndCapacityType>::assign(It start,
                                                                    It finish) {
  // Can't safely check std::distance for InputIterator, so use push_back().
  if constexpr (Derived::kFixedCapacity ||
                std::is_same_v<
                    typename std::iterator_traits<It>::iterator_category,
                    std::input_iterator_tag>) {
    clear();
    while (start != finish) {
      push_back(*start++);
    }
  } else {
    PW_ASSERT(try_assign(start, finish));
  }
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
bool GenericDeque<Derived, ValueType, CountAndCapacityType>::try_assign(
    size_type count, const value_type& value) {
  if (!CheckCapacity(count)) {
    return false;
  }
  clear();
  for (size_type i = 0; i < count; ++i) {
    EmplaceBackUnchecked(value);
  }
  return true;
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
template <typename It, int&..., typename>
bool GenericDeque<Derived, ValueType, CountAndCapacityType>::try_assign(
    It start, It finish) {
  // Requires at least a forward iterator to safely check std::distance.
  static_assert(std::is_convertible_v<
                typename std::iterator_traits<It>::iterator_category,
                std::forward_iterator_tag>);
  const auto items = std::distance(start, finish);
  PW_DASSERT(items >= 0);
  if (static_cast<std::make_unsigned_t<decltype(items)>>(items) >
          std::numeric_limits<size_type>::max() ||
      !CheckCapacity(static_cast<size_type>(items))) {
    return false;
  }

  clear();
  while (start != finish) {
    EmplaceBackUnchecked(*start++);
  }
  return true;
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
constexpr std::pair<span<const ValueType>, span<const ValueType>>
GenericDeque<Derived, ValueType, CountAndCapacityType>::contiguous_data()
    const {
  if (empty()) {
    return {span<const value_type>(), span<const value_type>()};
  }
  if (tail() > head()) {
    // If the newest entry is after the oldest entry, we have not wrapped:
    //     [  |head()|...more_entries...|tail()|  ]
    return {span<const value_type>(&data()[head()], size()),
            span<const value_type>()};
  } else {
    // If the newest entry is before or at the oldest entry and we know we are
    // not empty, ergo we have wrapped:
    //     [..more_entries...|tail()|  |head()|...more_entries...]
    return {span<const value_type>(&data()[head()], capacity() - head()),
            span<const value_type>(&data()[0], tail())};
  }
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
template <typename... Args>
bool GenericDeque<Derived, ValueType, CountAndCapacityType>::try_emplace_back(
    Args&&... args) {
  if (!CheckCapacityAddOne()) {
    return false;
  }
  EmplaceBackUnchecked(std::forward<Args>(args)...);
  return true;
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
void GenericDeque<Derived, ValueType, CountAndCapacityType>::pop_back() {
  PW_ASSERT(!empty());
  if constexpr (!std::is_trivially_destructible_v<value_type>) {
    std::destroy_at(&back());
  }
  Base::PopBack();
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
template <typename... Args>
bool GenericDeque<Derived, ValueType, CountAndCapacityType>::try_emplace_front(
    Args&&... args) {
  if (!CheckCapacityAddOne()) {
    return false;
  }
  Base::PushFront();
  new (&data()[head()]) value_type(std::forward<Args>(args)...);
  return true;
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
void GenericDeque<Derived, ValueType, CountAndCapacityType>::pop_front() {
  PW_ASSERT(!empty());
  if constexpr (!std::is_trivially_destructible_v<value_type>) {
    std::destroy_at(&front());
  }
  Base::PopFront();
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
bool GenericDeque<Derived, ValueType, CountAndCapacityType>::try_resize(
    size_type new_size, const value_type& value) {
  if (size() < new_size) {
    if (!CheckCapacity(new_size)) {
      return false;
    }
    const size_type new_items = new_size - size();
    for (size_type i = 0; i < new_items; ++i) {
      EmplaceBackUnchecked(value);
    }
  } else {
    while (size() > new_size) {
      pop_back();
    }
  }
  return true;
}

template <typename Derived, typename ValueType, typename CountAndCapacityType>
typename GenericDeque<Derived, ValueType, CountAndCapacityType>::iterator
GenericDeque<Derived, ValueType, CountAndCapacityType>::erase(
    const_iterator first, const_iterator last) {
  PW_DASSERT(first <= last);
  const iterator first_it(derived(), first.pos_);
  const iterator last_it(derived(), last.pos_);

  const size_type items_to_erase = static_cast<size_type>(last - first);
  if (items_to_erase == 0) {
    return first_it;
  }

  const size_type items_after = static_cast<size_type>(size() - last.pos_);
  // Shift head forward or tail backwards -- whichever involves fewer moves.
  if (first.pos_ < items_after) {  // fewer before than after
    std::move_backward(begin(), first_it, last_it);
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      std::destroy(begin(), begin() + items_to_erase);
    }
    Base::head_ = Base::AbsoluteIndex(items_to_erase);
  } else {  // fewer after than before
    std::move(last_it, end(), first_it);
    if constexpr (!std::is_trivially_destructible_v<value_type>) {
      std::destroy(first_it + items_after, end());
    }
    Base::tail_ = Base::AbsoluteIndex(first.pos_ + items_after);
  }
  Base::count_and_capacity().SetCount(size() - items_to_erase);
  return first_it;
}

}  // namespace pw::containers::internal
