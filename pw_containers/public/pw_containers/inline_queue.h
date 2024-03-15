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

#include <initializer_list>
#include <utility>

#include "pw_containers/inline_deque.h"

namespace pw {

template <typename T, typename SizeType, size_t kCapacity>
class BasicInlineQueue;

/// The `InlineQueue` class is similar to `std::queue<T, std::deque>`, except
/// it is backed by a fixed-size buffer. `InlineQueue`'s must be declared with
/// an explicit maximum size (e.g. `InlineQueue<int, 10>>`) but deques can be
/// used and referred to without the max size template parameter (e.g.
/// `InlineQueue<int>`).
///
/// `pw::InlineQueue` is wrapper around `pw::InlineDeque` with a simplified
/// API and `push_overwrite()` & `emplace_overwrite()` helpers.
template <typename T, size_t kCapacity = containers::internal::kGenericSized>
using InlineQueue = BasicInlineQueue<T, uint16_t, kCapacity>;

template <typename ValueType,
          typename SizeType,
          size_t kCapacity = containers::internal::kGenericSized>
class BasicInlineQueue
    : public BasicInlineQueue<ValueType,
                              SizeType,
                              containers::internal::kGenericSized> {
 private:
  using QueueBase = BasicInlineQueue<ValueType,
                                     SizeType,
                                     containers::internal::kGenericSized>;

 public:
  using typename QueueBase::const_iterator;
  using typename QueueBase::const_pointer;
  using typename QueueBase::const_reference;
  using typename QueueBase::difference_type;
  using typename QueueBase::iterator;
  using typename QueueBase::pointer;
  using typename QueueBase::reference;
  using typename QueueBase::size_type;
  using typename QueueBase::value_type;

  // Constructors

  constexpr BasicInlineQueue() noexcept : deque_() {}

  BasicInlineQueue(size_type count, const_reference value)
      : deque_(count, value) {}

  explicit BasicInlineQueue(size_type count) : deque_(count) {}

  template <
      typename InputIterator,
      typename = containers::internal::EnableIfInputIterator<InputIterator>>
  BasicInlineQueue(InputIterator start, InputIterator finish)
      : deque_(start, finish) {}

  BasicInlineQueue(const std::initializer_list<value_type>& list) {
    *this = list;
  }

  /// Copy constructs for matching capacity.
  BasicInlineQueue(const BasicInlineQueue& other) { *this = other; }

  /// Copy constructs for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineQueue(
      const BasicInlineQueue<ValueType, SizeType, kOtherCapacity>& other) {
    *this = other;
  }

  /// Move constructs for matching capacity.
  BasicInlineQueue(BasicInlineQueue&& other) { *this = std::move(other); }

  /// Move constructs for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineQueue(
      BasicInlineQueue<ValueType, SizeType, kOtherCapacity>&& other) {
    *this = std::move(other);
  }

  template <typename T, typename = containers::internal::EnableIfIterable<T>>
  BasicInlineQueue(const T& other) {
    *this = other;
  }

  //
  BasicInlineQueue& operator=(const std::initializer_list<value_type>& list) {
    deque_ = std::move(list);
    return *this;
  }

  /// Copy assigns from matching capacity.
  BasicInlineQueue& operator=(const BasicInlineQueue& other) {
    deque_ = other.deque_;
    return *this;
  }

  /// Copy assigns from mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineQueue& operator=(
      const BasicInlineQueue<ValueType, SizeType, kOtherCapacity>& other) {
    deque_ = other.deque_;
    return *this;
  }

  /// Move assigns from matching capacity.
  BasicInlineQueue& operator=(BasicInlineQueue&& other) {
    deque_ = std::move(other.deque_);
    return *this;
  }

  /// Move assigns from mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineQueue& operator=(
      BasicInlineQueue<ValueType, SizeType, kOtherCapacity>&& other) {
    deque_ = std::move(other.deque_);
    return *this;
  }

  template <typename T, typename = containers::internal::EnableIfIterable<T>>
  BasicInlineQueue& operator=(const T& other) {
    deque_ = BasicInlineDeque<ValueType, SizeType, kCapacity>(other.begin(),
                                                              other.end());
    return *this;
  }

  // Size

  static constexpr size_type max_size() { return capacity(); }
  static constexpr size_type capacity() { return kCapacity; }

 private:
  template <typename OtherValueType, typename OtherSizeType, size_t kOtherSized>
  friend class BasicInlineQueue;

  // The deque() function is defined differently for the generic-sized and
  // known-sized specializations. This data() implementation simply returns the
  // generic-sized deque_. The generic-sized deque() function casts *this to a
  // known zero-sized specialzation to access this exact function.
  BasicInlineDeque<ValueType, SizeType>& deque() { return deque_; }
  const BasicInlineDeque<ValueType, SizeType>& deque() const { return deque_; }

  BasicInlineDeque<ValueType, SizeType, kCapacity> deque_;
};

// Defines the generic-sized BasicInlineDeque<T> specialization, which
// serves as the base class for BasicInlineDeque<T> of any capacity.
//
// Except for constructors, all other methods should be implemented on this
// generic-sized specialization.
template <typename ValueType, typename SizeType>
class BasicInlineQueue<ValueType,
                       SizeType,
                       containers::internal::kGenericSized> {
 private:
  using Deque = BasicInlineDeque<ValueType, SizeType>;

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

  reference at(size_type index) { return deque().at(index); }
  const_reference at(size_type index) const { return deque().at(index); }

  reference operator[](size_type index) { return deque()[index]; }
  const_reference operator[](size_type index) const { return deque()[index]; }

  reference front() { return deque().front(); }
  const_reference front() const { return deque().front(); }

  reference back() { return deque().back(); }
  const_reference back() const { return deque().back(); }

  std::pair<span<const value_type>, span<const value_type>> contiguous_data()
      const {
    return deque().contiguous_data();
  }
  std::pair<span<value_type>, span<value_type>> contiguous_data() {
    return deque().contiguous_data();
  }

  // Iterate

  iterator begin() noexcept { return deque().begin(); }
  const_iterator begin() const noexcept { return cbegin(); }
  const_iterator cbegin() const noexcept { return deque().cbegin(); }

  iterator end() noexcept { return deque().end(); }
  const_iterator end() const noexcept { return cend(); }
  const_iterator cend() const noexcept { return deque().cend(); }

  // Size

  [[nodiscard]] bool empty() const noexcept { return deque().empty(); }

  [[nodiscard]] bool full() const noexcept { return deque().full(); }

  size_type size() const noexcept { return deque().size(); }

  size_type max_size() const noexcept { return capacity(); }

  size_type capacity() const noexcept { return deque().capacity(); }

  // Modify

  void clear() noexcept { deque().clear(); }

  void push(const value_type& value) { emplace(value); }

  void push(value_type&& value) { emplace(std::move(value)); }

  template <typename... Args>
  void emplace(Args&&... args) {
    deque().emplace_back(std::forward<Args>(args)...);
  }

  void push_overwrite(const value_type& value) { emplace_overwrite(value); }

  void push_overwrite(value_type&& value) {
    emplace_overwrite(std::move(value));
  }

  template <typename... Args>
  void emplace_overwrite(Args&&... args) {
    if (full()) {
      pop();
    }
    emplace(std::forward<Args>(args)...);
  }

  void pop() { deque().pop_front(); }

 protected:
  constexpr BasicInlineQueue() noexcept = default;

  // Polymorphic-sized `pw::InlineQueue<T>` may not be used with `unique_ptr`
  // or `delete`. `delete` could be supported using C++20's destroying delete.
  ~BasicInlineQueue() = default;

 private:
  // The underlying BasicInlineDeque is not part of the generic-sized class. It
  // is provided in the derived class from which this instance was constructed.
  // To access the data, down-cast this to a known max size specialization, and
  // return a reference to a generic-sized BasicInlineDeque, which is the same
  // reference for all sizes.
  BasicInlineDeque<ValueType, SizeType>& deque() {
    return static_cast<BasicInlineQueue<value_type, size_type, 0>*>(this)
        ->deque();
  }
  const BasicInlineDeque<ValueType, SizeType>& deque() const {
    return static_cast<const BasicInlineQueue<value_type, size_type, 0>*>(this)
        ->deque();
  }
};

}  // namespace pw
