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

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <type_traits>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_containers/internal/generic_deque.h"
#include "pw_containers/internal/raw_storage.h"
#include "pw_toolchain/constexpr_tag.h"

namespace pw {

template <typename T, typename SizeType, size_t kCapacity>
class BasicInlineDeque;

// Storage for a queue's data and that ensures entries are `clear`'d before
// the storage is removed.
template <typename ValueType,
          typename SizeType,
          size_t kCapacity,
          bool kIsTriviallyDestructible>
class BasicInlineDequeStorage;

/// The `InlineDeque` class is similar to the STL's double ended queue
/// (`std::deque`), except it is backed by a fixed-size buffer.
/// `InlineDeque`'s must be declared with an explicit maximum size (e.g.
/// `InlineDeque<int, 10>>`) but deques can be used and referred to without
/// the max size template parameter (e.g. `InlineDeque<int>`).
///
/// To allow referring to a `pw::InlineDeque` without an explicit maximum size,
/// all `InlineDeque` classes inherit from the `BasicInlineDequeStorage` class,
/// which in turn inherits from `InlineDeque<T>`, which stores the maximum size
/// in a variable. This allows `InlineDeque`s to be used without having to know
/// their maximum size at compile time. It also keeps code size small since
/// function implementations are shared for all maximum sizes.
///
/// An `InlineDeque` cannot increase its capacity. Any operations that would
/// exceed the capacity (e.g. `assign`, `push_back`, `push_front`) fail a
/// `PW_ASSERT`. Avoid this by choosing a large enough capacity or checking
/// `full()` before adding items.
template <typename T, size_t kCapacity = containers::internal::kGenericSized>
using InlineDeque = BasicInlineDeque<T, uint16_t, kCapacity>;

template <typename ValueType,
          typename SizeType,
          size_t kCapacity = containers::internal::kGenericSized>
class BasicInlineDeque : public BasicInlineDequeStorage<
                             ValueType,
                             SizeType,
                             kCapacity,
                             std::is_trivially_destructible_v<ValueType>> {
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

  /// Constructs with zero elements.
  BasicInlineDeque() noexcept {}

  // Explicit zero element constexpr constructor. Using this constructor will
  // place the entire object in .data, which will increase ROM size. Use with
  // caution if working with large capacity sizes.
  constexpr BasicInlineDeque(ConstexprTag /*constexpr_tag*/) noexcept {}

  /// Constructs with ``count`` copies of ``value``.
  BasicInlineDeque(size_type count, const_reference value) {
    Base::assign(count, value);
  }

  /// Constructs with ``count`` default-initialized elements.
  explicit BasicInlineDeque(size_type count)
      : BasicInlineDeque(count, value_type()) {}

  /// Copy constructs from an iterator.
  template <
      typename InputIterator,
      typename = containers::internal::EnableIfInputIterator<InputIterator>>
  BasicInlineDeque(InputIterator start, InputIterator finish) {
    Base::assign(start, finish);
  }

  /// Copy constructs for matching capacity.
  BasicInlineDeque(const BasicInlineDeque& other) { *this = other; }

  /// Copy constructs for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineDeque(
      const BasicInlineDeque<ValueType, SizeType, kOtherCapacity>& other) {
    *this = other;
  }

  /// Move constructs for matching capacity.
  BasicInlineDeque(BasicInlineDeque&& other) noexcept {
    *this = std::move(other);
  }

  /// Move constructs for mismatched capacity.
  template <size_t kOtherCapacity>
  BasicInlineDeque(
      BasicInlineDeque<ValueType, SizeType, kOtherCapacity>&& other) noexcept {
    *this = std::move(other);
  }

  /// Copy constructs from an initializer list.
  BasicInlineDeque(const std::initializer_list<value_type>& list) {
    *this = list;
  }

  /// Copy constructor for arbitrary iterables.
  template <typename T, typename = containers::internal::EnableIfIterable<T>>
  BasicInlineDeque(const T& other) {
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
  BasicInlineDeque& operator=(const std::initializer_list<value_type>& list) {
    Base::operator=(list);
    return *this;
  }

  /// Copy assigns for matching capacity.
  BasicInlineDeque& operator=(const BasicInlineDeque& other) {
    Base::operator=(static_cast<const Base&>(other));
    return *this;
  }

  /// Copy assigns for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineDeque& operator=(
      const BasicInlineDeque<ValueType, SizeType, kOtherCapacity>& other) {
    Base::operator=(static_cast<const Base&>(other));
    return *this;
  }

  /// Move assigns for matching capacity.
  BasicInlineDeque& operator=(BasicInlineDeque&& other) noexcept {
    Base::operator=(static_cast<Base&&>(std::move(other)));
    return *this;
  }

  /// Move assigns for mismatched capacity.
  template <size_t kOtherCapacity>
  BasicInlineDeque& operator=(
      BasicInlineDeque<ValueType, SizeType, kOtherCapacity>&& other) noexcept {
    Base::operator=(static_cast<Base&&>(std::move(other)));
    return *this;
  }

  template <typename T, typename = containers::internal::EnableIfIterable<T>>
  BasicInlineDeque& operator=(const T& other) {
    Base::operator=(other);
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
};

// Specialization of ``BasicInlineDequeue`` for trivially-destructible
// ``ValueType``. This specialization ensures that no destructor is generated.
template <typename ValueType, typename SizeType, size_t kCapacity>
class BasicInlineDequeStorage<ValueType, SizeType, kCapacity, true>
    : public BasicInlineDeque<ValueType,
                              SizeType,
                              containers::internal::kGenericSized> {
  // NOTE: no destructor is added, as `ValueType` is trivially-destructible.
 private:
  friend class BasicInlineDeque<ValueType, SizeType, kCapacity>;
  friend class BasicInlineDeque<ValueType,
                                SizeType,
                                containers::internal::kGenericSized>;

  using Base = BasicInlineDeque<ValueType,
                                SizeType,
                                containers::internal::kGenericSized>;

  constexpr BasicInlineDequeStorage() : Base(kCapacity) {}

  // The data() function is defined differently for the generic-sized and
  // known-sized specializations. This data() implementation simply returns the
  // RawStorage's data(). The generic-sized data() function casts *this to a
  // known zero-sized specialization to access this exact function.
  typename Base::pointer data() { return raw_storage_.data(); }
  typename Base::const_pointer data() const { return raw_storage_.data(); }

  // Note that this is offset and aligned the same for all possible
  // kCapacity values for the same value_type.
  containers::internal::RawStorage<ValueType, kCapacity> raw_storage_;
};

// Specialization of ``BasicInlineDequeue`` for non-trivially-destructible
// ``ValueType``. This specialization ensures that the queue is cleared
// during destruction prior to the invalidation of the `raw_storage_`.
template <typename ValueType, typename SizeType, size_t kCapacity>
class BasicInlineDequeStorage<ValueType, SizeType, kCapacity, false>
    : public BasicInlineDeque<ValueType,
                              SizeType,
                              containers::internal::kGenericSized> {
 public:
  ~BasicInlineDequeStorage() { Base::clear(); }

 private:
  friend class BasicInlineDeque<ValueType, SizeType, kCapacity>;
  friend class BasicInlineDeque<ValueType,
                                SizeType,
                                containers::internal::kGenericSized>;

  using Base = BasicInlineDeque<ValueType,
                                SizeType,
                                containers::internal::kGenericSized>;

  constexpr BasicInlineDequeStorage() : Base(kCapacity) {}

  // The data() function is defined differently for the generic-sized and
  // known-sized specializations. This data() implementation simply returns the
  // RawStorage's data(). The generic-sized data() function casts *this to a
  // known zero-sized specialization to access this exact function.
  typename Base::pointer data() { return raw_storage_.data(); }
  typename Base::const_pointer data() const { return raw_storage_.data(); }

  // Note that this is offset and aligned the same for all possible
  // kCapacity values for the same value_type.
  containers::internal::RawStorage<ValueType, kCapacity> raw_storage_;
};

// Defines the generic-sized BasicInlineDeque<T> specialization, which
// serves as the base class for BasicInlineDeque<T> of any capacity.
//
// Except for constructors and destructors, all other methods should be
// implemented on this generic-sized specialization. Destructors must
// only be written for the `BasicInlineDequeStorage` type in order to ensure
// that `raw_storage_` is still valid at the time of destruction.
//
// NOTE: this size-polymorphic base class must not be used inside of
// ``std::unique_ptr`` or ``delete``.
template <typename ValueType, typename SizeType>
class BasicInlineDeque<ValueType, SizeType, containers::internal::kGenericSized>
    : public containers::internal::GenericDeque<
          BasicInlineDeque<ValueType,
                           SizeType,
                           containers::internal::kGenericSized>,
          ValueType,
          SizeType> {
 private:
  using Base = containers::internal::GenericDeque<
      BasicInlineDeque<ValueType,
                       SizeType,
                       containers::internal::kGenericSized>,
      ValueType,
      SizeType>;

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

  // Polymorphic-sized `pw::InlineDeque<T>` may not be constructed directly.
  BasicInlineDeque() = delete;

  BasicInlineDeque& operator=(const BasicInlineDeque& other) {
    Base::assign(other.begin(), other.end());
    return *this;
  }

  BasicInlineDeque& operator=(BasicInlineDeque&& other) noexcept {
    Base::clear();
    for (auto&& item : other) {
      Base::emplace_back(std::move(item));
    }
    other.clear();
    return *this;
  }

  using Base::operator=;

  size_type max_size() const noexcept { return Base::capacity(); }

  [[nodiscard]] bool full() const noexcept {
    return Base::size() == Base::capacity();
  }

 protected:
  constexpr BasicInlineDeque(size_type capacity) noexcept : Base(capacity) {}

  // Polymorphic-sized `pw::InlineDeque<T>` may not be used with `unique_ptr`
  // or `delete`. `delete` could be supported using C++20's destroying delete.
  ~BasicInlineDeque() = default;

 private:
  friend Base;

  static constexpr bool kFixedCapacity = true;  // uses static allocation

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
};

}  // namespace pw
