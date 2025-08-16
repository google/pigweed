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
#include <cstdint>

#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/try.h"
#include "pw_containers/inline_deque.h"
#include "pw_containers/internal/async_count_and_capacity.h"
#include "pw_containers/internal/raw_storage.h"
#include "pw_toolchain/constexpr_tag.h"

namespace pw {

/// @module{pw_containers}

// Forward declaration.
template <typename T, typename SizeType, size_t kCapacity>
class BasicInlineAsyncDeque;

/// Async wrapper around BasicInlineDeque.
///
/// This class mimics the structure of `BasicInlineDequeImpl` to allow referring
/// to an `InlineAsyncDeque` without an explicit maximum size.
template <typename T, size_t kCapacity = containers::internal::kGenericSized>
using InlineAsyncDeque = BasicInlineAsyncDeque<T, uint16_t, kCapacity>;

template <typename ValueType,
          typename SizeType,
          size_t kCapacity = containers::internal::kGenericSized>
class BasicInlineAsyncDeque
    : public containers::internal::RawStorage<
          BasicInlineAsyncDeque<ValueType,
                                SizeType,
                                containers::internal::kGenericSized>,
          ValueType,
          kCapacity> {
 private:
  using Base = BasicInlineAsyncDeque<ValueType,
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

  /// Constructs with zero elements.
  constexpr BasicInlineAsyncDeque() noexcept = default;

  /// Constructs with ``count`` copies of ``value``.
  BasicInlineAsyncDeque(size_type count, const_reference value) {
    Base::assign(count, value);
  }

  /// Constructs with ``count`` default-initialized elements.
  explicit BasicInlineAsyncDeque(size_type count)
      : BasicInlineAsyncDeque(count, value_type()) {}

  /// Copy constructs from an iterator.
  template <
      typename InputIterator,
      typename = containers::internal::EnableIfInputIterator<InputIterator>>
  BasicInlineAsyncDeque(InputIterator start, InputIterator finish) {
    Base::assign(start, finish);
  }

  /// Copy constructs for matching capacity.
  BasicInlineAsyncDeque(const BasicInlineAsyncDeque& other) { *this = other; }

  /// Copy constructs for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineAsyncDeque(
      const BasicInlineAsyncDeque<ValueType, SizeType, kOtherCapacity>& other) {
    *this = other;
  }

  /// Move constructs for matching capacity.
  BasicInlineAsyncDeque(BasicInlineAsyncDeque&& other) noexcept {
    *this = std::move(other);
  }

  /// Move constructs for mismatched capacity.
  template <size_t kOtherCapacity>
  BasicInlineAsyncDeque(
      BasicInlineAsyncDeque<ValueType, SizeType, kOtherCapacity>&&
          other) noexcept {
    *this = std::move(other);
  }

  /// Copy constructs from an initializer list.
  BasicInlineAsyncDeque(const std::initializer_list<value_type>& list) {
    *this = list;
  }

  /// Copy constructor for arbitrary iterables.
  template <typename T, typename = containers::internal::EnableIfIterable<T>>
  BasicInlineAsyncDeque(const T& other) {
    *this = other;
  }

  // Assignment operators
  //
  // These operators delegate to the base class implementations in order to
  // maximize code reuse. The wrappers are required so that `operator=`
  // returns the correct type of reference.
  //
  // The `static_cast`s below are unfortunately necessary: without them,
  // overload resolution prefers to use the "iterable" operators rather than
  // upcast the RHS.

  /// Copy assigns from ``list``.
  BasicInlineAsyncDeque& operator=(
      const std::initializer_list<value_type>& list) {
    Base::operator=(list);
    return *this;
  }

  /// Copy assigns for matching capacity.
  BasicInlineAsyncDeque& operator=(const BasicInlineAsyncDeque& other) {
    Base::operator=(static_cast<const Base&>(other));
    return *this;
  }

  /// Copy assigns for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineAsyncDeque& operator=(
      const BasicInlineAsyncDeque<ValueType, SizeType, kOtherCapacity>& other) {
    Base::operator=(static_cast<const Base&>(other));
    return *this;
  }

  /// Move assigns for matching capacity.
  BasicInlineAsyncDeque& operator=(BasicInlineAsyncDeque&& other) noexcept {
    Base::operator=(static_cast<Base&&>(std::move(other)));
    return *this;
  }

  /// Move assigns for mismatched capacity.
  template <size_t kOtherCapacity>
  BasicInlineAsyncDeque& operator=(
      BasicInlineAsyncDeque<ValueType, SizeType, kOtherCapacity>&&
          other) noexcept {
    Base::operator=(static_cast<Base&&>(std::move(other)));
    return *this;
  }

  // Size

  static constexpr size_type max_size() { return capacity(); }
  static constexpr size_type capacity() { return kCapacity; }
};

// Defines the generic-sized BasicInlineAsyncDeque<T> specialization, which
// serves as the base class for BasicInlineAsyncDeque<T> of any capacity.
//
// Except for constructors and destructors, all other methods should be
// implemented on this generic-sized specialization.
//
// NOTE: this size-polymorphic base class must not be used inside of
// ``std::unique_ptr`` or ``delete``.
template <typename ValueType, typename SizeType>
class BasicInlineAsyncDeque<ValueType,
                            SizeType,
                            containers::internal::kGenericSized>
    : public containers::internal::BasicInlineDequeImpl<
          ValueType,
          containers::internal::AsyncCountAndCapacity<SizeType>,
          containers::internal::kGenericSized> {
 private:
  using Base = containers::internal::BasicInlineDequeImpl<
      ValueType,
      containers::internal::AsyncCountAndCapacity<SizeType>,
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

  BasicInlineAsyncDeque& operator=(const BasicInlineAsyncDeque& other) {
    Base::assign(other.begin(), other.end());
    return *this;
  }

  BasicInlineAsyncDeque& operator=(BasicInlineAsyncDeque&& other) noexcept {
    Base::clear();
    for (auto&& item : other) {
      Base::emplace_back(std::move(item));
    }
    other.clear();
    return *this;
  }

  using Base::operator=;

  /// Returns `Pending` until space for `num` elements is available.
  async2::Poll<> PendHasSpace(async2::Context& context, size_type num = 1) {
    return Base::count_and_capacity().PendHasSpace(context, num);
  }

  /// Returns `Pending` until an element is available.
  async2::Poll<> PendNotEmpty(async2::Context& context) {
    return Base::count_and_capacity().PendNotEmpty(context);
  }

 protected:
  constexpr BasicInlineAsyncDeque(size_type capacity) noexcept
      : Base(capacity) {}

  // Polymorphic-sized `InlineAsyncDeque` may not be used with `unique_ptr`
  // or `delete`. `delete` could be supported using C++20's destroying delete.
  ~BasicInlineAsyncDeque() = default;

 private:
  static constexpr bool kFixedCapacity = true;  // uses static allocation
};

}  // namespace pw
