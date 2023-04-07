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

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_containers/internal/raw_storage.h"
#include "pw_span/span.h"

namespace pw {
namespace inline_circular_buffer_impl {

template <typename ValueType, typename SizeType>
class InlineDequeIterator;

}  // namespace inline_circular_buffer_impl

template <typename T, typename SizeType, size_t kCapacity>
class BasicInlineDeque;

/// The `InlineDeque` class is similar to the STL's double ended queue
/// (`std::deque`), except it is backed by a fixed-size buffer.
/// `InlineDeque`'s must be declared with an explicit maximum size (e.g.
/// `InlineDeque<int, 10>>`) but deques can be used and referred to without
/// the max size template parameter (e.g. `InlineDeque<int>`).
///
/// To allow referring to a `pw::InlineDeque` without an explicit maximum
/// size, all `InlineDeque` classes inherit from the generic
/// `InlineDeque<T>`, which stores the maximum size in a variable. This allows
/// InlineDeques to be used without having to know their maximum size at compile
/// time. It also keeps code size small since function implementations are
/// shared for all maximum sizes.
template <typename T, size_t kCapacity = containers::internal::kGenericSized>
using InlineDeque = BasicInlineDeque<T, uint16_t, kCapacity>;

template <typename ValueType,
          typename SizeType,
          size_t kCapacity = containers::internal::kGenericSized>
class BasicInlineDeque
    : public BasicInlineDeque<ValueType,
                              SizeType,
                              containers::internal::kGenericSized> {
 private:
  using Base = BasicInlineDeque<ValueType,
                                SizeType,
                                containers::internal::kGenericSized>;

 public:
  using typename Base::const_iterator;
  using typename Base::const_pointer;
  using typename Base::const_reference;
  using typename Base::difference_type;
  using typename Base::iterator;
  using typename Base::pointer;
  using typename Base::reference;
  using typename Base::size_type;
  using typename Base::value_type;

  // Constructors

  constexpr BasicInlineDeque() noexcept : Base(kCapacity) {}

  BasicInlineDeque(size_type count, const_reference value) : Base(kCapacity) {
    Base::assign(count, value);
  }

  explicit BasicInlineDeque(size_type count)
      : BasicInlineDeque(count, value_type()) {}

  template <
      typename InputIterator,
      typename = containers::internal::EnableIfInputIterator<InputIterator>>
  BasicInlineDeque(InputIterator start, InputIterator finish)
      : Base(kCapacity) {
    Base::assign(start, finish);
  }

  template <size_type kOtherCapacity>
  BasicInlineDeque(
      const BasicInlineDeque<value_type, size_type, kOtherCapacity>& other)
      : BasicInlineDeque(other.start(), other.end()) {
    static_assert(kCapacity >= kOtherCapacity);
  }

  BasicInlineDeque(std::initializer_list<value_type> list)
      : BasicInlineDeque(list.begin(), list.end()) {}

  // Assignment

  BasicInlineDeque& operator=(const BasicInlineDeque& other) = default;

  // Checks capacity rather than current size.
  template <size_type kOtherCapacity>
  BasicInlineDeque& operator=(
      const BasicInlineDeque<value_type, size_type, kOtherCapacity>& other) {
    Base::template assign<kOtherCapacity>(other);
    return *this;
  }

  BasicInlineDeque& operator=(std::initializer_list<value_type> list) {
    Base::assign(list);
    return *this;
  }

  // Size

  static constexpr size_type max_size() { return capacity(); }
  static constexpr size_type capacity() { return kCapacity; }

  // All other methods are implemented on the generic-sized base class.

 private:
  friend class BasicInlineDeque<value_type,
                                size_type,
                                containers::internal::kGenericSized>;

  static_assert(kCapacity <= std::numeric_limits<size_type>::max());

  // The data() function is defined differently for the generic-sized and
  // known-sized specializations. This data() implementation simply returns the
  // RawStorage's data(). The generic-sized data() function casts *this to a
  // known zero-sized specialization to access this exact function.
  pointer data() { return raw_storage_.data(); }
  const_pointer data() const { return raw_storage_.data(); }

  // Note that this is offset and aligned the same for all possible
  // kCapacity values for the same value_type.
  containers::internal::RawStorage<value_type, kCapacity> raw_storage_;
};

// Defines the generic-sized BasicInlineDeque<T> specialization, which
// serves as the base class for BasicInlineDeque<T> of any capacity.
//
// Except for constructors, all other methods should be implemented on this
// generic-sized specialization.
template <typename ValueType, typename SizeType>
class BasicInlineDeque<ValueType, SizeType, containers::internal::kGenericSized>
    : public containers::internal::DestructorHelper<
          BasicInlineDeque<ValueType,
                           SizeType,
                           containers::internal::kGenericSized>,
          std::is_trivially_destructible<ValueType>::value> {
 public:
  using value_type = ValueType;
  using size_type = SizeType;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator =
      inline_circular_buffer_impl::InlineDequeIterator<value_type, size_type>;
  using const_iterator =
      inline_circular_buffer_impl::InlineDequeIterator<const value_type,
                                                       size_type>;

  // Assignment

  BasicInlineDeque& operator=(std::initializer_list<value_type> list) {
    assign(list);
    return *this;
  }

  void assign(size_type count, const value_type& value) {
    clear();
    Append(count, value);
  }

  template <
      typename InputIterator,
      typename = containers::internal::EnableIfInputIterator<InputIterator>>
  void assign(InputIterator start, InputIterator finish) {
    clear();
    CopyFrom(start, finish);
  }

  void assign(std::initializer_list<value_type> list) {
    assign(list.begin(), list.end());
  }

  // Access

  reference at(size_type index) {
    PW_ASSERT(index < size());
    return data()[AbsoluteIndex(index)];
  }
  const_reference at(size_type index) const {
    PW_ASSERT(index < size());
    return data()[AbsoluteIndex(index)];
  }

  reference operator[](size_type index) {
    PW_DASSERT(index < size());
    return data()[AbsoluteIndex(index)];
  }
  const_reference operator[](size_type index) const {
    PW_DASSERT(index < size());
    return data()[AbsoluteIndex(index)];
  }

  reference front() {
    PW_DASSERT(!empty());
    return data()[head_];
  }
  const_reference front() const {
    PW_DASSERT(!empty());
    return data()[head_];
  }

  reference back() {
    PW_DASSERT(!empty());
    return data()[AbsoluteIndex(count_ - 1)];
  }
  const_reference back() const {
    PW_DASSERT(!empty());
    return data()[AbsoluteIndex(count_ - 1)];
  }

  // Provides access to the valid data in a contiguous form.
  std::pair<span<const value_type>, span<const value_type>> contiguous_data()
      const;
  std::pair<span<value_type>, span<value_type>> contiguous_data() {
    auto [first, second] =
        static_cast<const BasicInlineDeque&>(*this).contiguous_data();
    return std::make_pair(
        span<value_type>(const_cast<pointer>(first.data()), first.size()),
        span<value_type>(const_cast<pointer>(second.data()), second.size()));
  }

  // Iterate

  iterator begin() noexcept {
    if (empty()) {
      return end();
    }
    return iterator(data(), capacity(), data() + head_, count_);
  }
  const_iterator begin() const noexcept { return cbegin(); }
  const_iterator cbegin() const noexcept {
    if (empty()) {
      return cend();
    }
    return const_iterator(data(), capacity(), data() + head_, count_);
  }

  iterator end() noexcept {
    return iterator(data(), capacity(), data() + capacity(), count_);
  }
  const_iterator end() const noexcept { return cend(); }
  const_iterator cend() const noexcept {
    return const_iterator(data(), capacity(), data() + capacity(), count_);
  }

  // Size

  [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

  [[nodiscard]] bool full() const noexcept { return size() == max_size(); }

  size_type size() const noexcept { return count_; }

  size_type max_size() const noexcept { return capacity(); }

  size_type capacity() const noexcept { return capacity_; }

  // Modify

  void clear() noexcept;

  void push_back(const value_type& value) { emplace_back(value); }

  void push_back(value_type&& value) { emplace_back(std::move(value)); }

  template <typename... Args>
  void emplace_back(Args&&... args);

  void pop_back();

  void push_front(const value_type& value) { emplace_front(value); }

  void push_front(value_type&& value) { emplace_front(std::move(value)); }

  template <typename... Args>
  void emplace_front(Args&&... args);

  void pop_front();

  void resize(size_type new_size) { resize(new_size, value_type()); }

  void resize(size_type new_size, const value_type& value);

 protected:
  constexpr BasicInlineDeque(size_type capacity) noexcept
      : capacity_(capacity), head_(0), tail_(0), count_(0) {}

 private:
  // The underlying RawStorage is not part of the generic-sized class. It is
  // provided in the derived class from which this instance was constructed. To
  // access the data, down-cast this to a known max size specialization, and
  // return the RawStorage's data, which is the same for all sizes.
  pointer data() {
    return static_cast<BasicInlineDeque<value_type, size_type, 0>*>(this)
        ->data();
  }
  const_pointer data() const {
    return static_cast<const BasicInlineDeque<value_type, size_type, 0>*>(this)
        ->data();
  }

  void IncrementWithWrap(size_type& index) const {
    index++;
    // Note: branch is faster than mod (%) on common embedded
    // architectures.
    if (index == max_size()) {
      index = 0;
    }
  }

  void DecrementWithWrap(size_type& index) const {
    if (index == 0) {
      index = max_size();
    }
    index--;
  }

  // Returns the absolute index based on the relative index beyond the
  // head offset.
  //
  // Precondition: The relative index must be valid, i.e. < size().
  size_type AbsoluteIndex(const size_type relative_index) const {
    const size_type absolute_index = head_ + relative_index;
    if (absolute_index < max_size()) {
      return absolute_index;
    }
    // Offset wrapped across the end of the circular buffer.
    return absolute_index - max_size();
  }

  template <typename Iterator>
  void CopyFrom(Iterator start, Iterator finish);

  void Append(size_type count, const value_type& value);

  const size_type capacity_;
  size_type head_;  // Non-inclusive offset for the front.
  size_type tail_;  // Non-inclusive offset for the back.
  size_type count_;
};

// Function implementations

template <typename ValueType, typename SizeType>
std::pair<span<const ValueType>, span<const ValueType>>
BasicInlineDeque<ValueType, SizeType>::contiguous_data() const {
  if (empty()) {
    return std::make_pair(span<const value_type>(), span<const value_type>());
  }
  if (tail_ > head_) {
    // If the newest entry is after the oldest entry, we have not wrapped:
    //     [  |head_|...more_entries...|tail_|  ]
    return std::make_pair(span<const value_type>(&data()[head_], count_),
                          span<const value_type>());
  } else {
    // If the newest entry is before or at the oldest entry and we know we are
    // not empty, ergo we have wrapped:
    //     [..more_entries...|tail_|  |head_|...more_entries...]
    return std::make_pair(
        span<const value_type>(&data()[head_], max_size() - head_),
        span<const value_type>(&data()[0], tail_));
  }
}

template <typename ValueType, typename SizeType>
void BasicInlineDeque<ValueType, SizeType>::clear() noexcept {
  if constexpr (!std::is_trivially_destructible_v<value_type>) {
    for (auto& item : *this) {
      item.~value_type();
    }
  }
  head_ = 0;
  tail_ = 0;
  count_ = 0;
}

template <typename ValueType, typename SizeType>
template <typename... Args>
void BasicInlineDeque<ValueType, SizeType>::emplace_back(Args&&... args) {
  PW_ASSERT(!full());
  PW_DASSERT(tail_ < capacity());
  new (&data()[tail_]) value_type(std::forward<Args>(args)...);
  IncrementWithWrap(tail_);
  ++count_;
}

template <typename ValueType, typename SizeType>
void BasicInlineDeque<ValueType, SizeType>::pop_back() {
  PW_ASSERT(!empty());
  PW_DASSERT(tail_ < capacity());
  if constexpr (!std::is_trivially_destructible_v<value_type>) {
    back().~value_type();
  }
  DecrementWithWrap(tail_);
  --count_;
}

template <typename ValueType, typename SizeType>
template <typename... Args>
void BasicInlineDeque<ValueType, SizeType>::emplace_front(Args&&... args) {
  PW_ASSERT(!full());
  DecrementWithWrap(head_);
  PW_DASSERT(head_ < capacity());
  new (&data()[head_]) value_type(std::forward<Args>(args)...);
  ++count_;
}

template <typename ValueType, typename SizeType>
void BasicInlineDeque<ValueType, SizeType>::pop_front() {
  PW_ASSERT(!empty());
  PW_DASSERT(head_ < capacity());
  if constexpr (!std::is_trivially_destructible_v<value_type>) {
    front().~value_type();
  }
  IncrementWithWrap(head_);
  --count_;
}

template <typename ValueType, typename SizeType>
void BasicInlineDeque<ValueType, SizeType>::resize(size_type new_size,
                                                   const value_type& value) {
  if (size() < new_size) {
    Append(std::min(static_cast<size_t>(max_size()),
                    static_cast<size_t>(new_size)) -
               size(),
           value);
  } else {
    while (size() > new_size) {
      pop_back();
    }
  }
}

template <typename ValueType, typename SizeType>
template <typename Iterator>
void BasicInlineDeque<ValueType, SizeType>::CopyFrom(Iterator start,
                                                     Iterator finish) {
  while (start != finish) {
    push_back(*start++);
  }
}

template <typename ValueType, typename SizeType>
void BasicInlineDeque<ValueType, SizeType>::Append(size_type count,
                                                   const value_type& value) {
  for (size_type i = 0; i < count; ++i) {
    push_back(value);
  }
}

namespace inline_circular_buffer_impl {

template <typename ValueType, typename SizeType>
class InlineDequeIterator {
 public:
  using value_type = ValueType;
  using size_type = SizeType;
  using reference = value_type&;
  using pointer = value_type*;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::forward_iterator_tag;

  constexpr InlineDequeIterator() = default;
  constexpr InlineDequeIterator(pointer data,
                                size_type capacity,
                                pointer current,
                                size_type count)
      : data_begin_(data),
        data_end_(data + capacity),
        current_(current),
        count_(count) {}

  InlineDequeIterator(const InlineDequeIterator& other) = default;
  InlineDequeIterator& operator=(const InlineDequeIterator& other) = default;

  InlineDequeIterator& operator++() {
    PW_DASSERT(count_ != 0);
    --count_;
    if (count_ == 0) {
      current_ = data_end_;  // Jump to end() to stop iteration.
      return *this;
    }
    ++current_;
    if (current_ == data_end_) {
      current_ = data_begin_;  // We just wrapped.
    }
    return *this;
  }

  InlineDequeIterator operator++(int) {
    InlineDequeIterator it(*this);
    operator++();
    return it;
  }

  constexpr const value_type& operator*() const { return *current_; }
  constexpr value_type& operator*() { return *current_; }

  constexpr const value_type* operator->() const { return current_; }
  constexpr value_type* operator->() { return current_; }

  bool operator==(const InlineDequeIterator& other) const {
    return (current_ == other.current_);
  }

  bool operator!=(const InlineDequeIterator& other) const {
    return (current_ != other.current_);
  }

 private:
  pointer data_begin_;
  pointer data_end_;
  pointer current_;
  size_type count_;
};

}  // namespace inline_circular_buffer_impl
}  // namespace pw
