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
#include "pw_containers/internal/count_and_capacity.h"
#include "pw_containers/internal/generic_deque.h"
#include "pw_containers/internal/raw_storage.h"
#include "pw_toolchain/constexpr_tag.h"

namespace pw {
namespace containers::internal {

// Forward declarations.
template <typename T, typename CountAndCapacityType, size_t kCapacity>
class BasicInlineDequeImpl;

}  // namespace containers::internal

template <typename ValueType,
          typename SizeType,
          size_t kCapacity = containers::internal::kGenericSized>
using BasicInlineDeque = containers::internal::BasicInlineDequeImpl<
    ValueType,
    containers::internal::CountAndCapacity<SizeType>,
    kCapacity>;

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

namespace containers::internal {

// Storage for a queue's data and that ensures entries are `clear`'d before
// the storage is removed.
template <typename ValueType,
          typename CountAndCapacityType,
          size_t kCapacity,
          bool kIsTriviallyDestructible>
class BasicInlineDequeStorage;

template <typename ValueType, typename CountAndCapacityType, size_t kCapacity>
class BasicInlineDequeImpl : public BasicInlineDequeStorage<
                                 ValueType,
                                 CountAndCapacityType,
                                 kCapacity,
                                 std::is_trivially_destructible_v<ValueType>> {
 private:
  using Base =
      BasicInlineDequeImpl<ValueType, CountAndCapacityType, kGenericSized>;

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
  BasicInlineDequeImpl() noexcept {}

  // Explicit zero element constexpr constructor. Using this constructor will
  // place the entire object in .data, which will increase ROM size. Use with
  // caution if working with large capacity sizes.
  constexpr BasicInlineDequeImpl(ConstexprTag /*constexpr_tag*/) noexcept {}

  /// Constructs with ``count`` copies of ``value``.
  BasicInlineDequeImpl(size_type count, const_reference value) {
    Base::assign(count, value);
  }

  /// Constructs with ``count`` default-initialized elements.
  explicit BasicInlineDequeImpl(size_type count)
      : BasicInlineDequeImpl(count, value_type()) {}

  /// Copy constructs from an iterator.
  template <typename InputIterator,
            typename = EnableIfInputIterator<InputIterator>>
  BasicInlineDequeImpl(InputIterator start, InputIterator finish) {
    Base::assign(start, finish);
  }

  /// Copy constructs for matching capacity.
  BasicInlineDequeImpl(const BasicInlineDequeImpl& other) { *this = other; }

  /// Copy constructs for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineDequeImpl(const BasicInlineDequeImpl<ValueType,
                                                  CountAndCapacityType,
                                                  kOtherCapacity>& other) {
    *this = other;
  }

  /// Move constructs for matching capacity.
  BasicInlineDequeImpl(BasicInlineDequeImpl&& other) noexcept {
    *this = std::move(other);
  }

  /// Move constructs for mismatched capacity.
  template <size_t kOtherCapacity>
  BasicInlineDequeImpl(
      BasicInlineDequeImpl<ValueType, CountAndCapacityType, kOtherCapacity>&&
          other) noexcept {
    *this = std::move(other);
  }

  /// Copy constructs from an initializer list.
  BasicInlineDequeImpl(const std::initializer_list<value_type>& list) {
    *this = list;
  }

  /// Copy constructor for arbitrary iterables.
  template <typename T, typename = EnableIfIterable<T>>
  BasicInlineDequeImpl(const T& other) {
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
  BasicInlineDequeImpl& operator=(
      const std::initializer_list<value_type>& list) {
    Base::operator=(list);
    return *this;
  }

  /// Copy assigns for matching capacity.
  BasicInlineDequeImpl& operator=(const BasicInlineDequeImpl& other) {
    Base::operator=(static_cast<const Base&>(other));
    return *this;
  }

  /// Copy assigns for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineDequeImpl& operator=(
      const BasicInlineDequeImpl<ValueType,
                                 CountAndCapacityType,
                                 kOtherCapacity>& other) {
    Base::operator=(static_cast<const Base&>(other));
    return *this;
  }

  /// Move assigns for matching capacity.
  BasicInlineDequeImpl& operator=(BasicInlineDequeImpl&& other) noexcept {
    Base::operator=(static_cast<Base&&>(std::move(other)));
    return *this;
  }

  /// Move assigns for mismatched capacity.
  template <size_t kOtherCapacity>
  BasicInlineDequeImpl& operator=(
      BasicInlineDequeImpl<ValueType, CountAndCapacityType, kOtherCapacity>&&
          other) noexcept {
    Base::operator=(static_cast<Base&&>(std::move(other)));
    return *this;
  }

  template <typename T, typename = EnableIfIterable<T>>
  BasicInlineDequeImpl& operator=(const T& other) {
    Base::operator=(other);
    return *this;
  }

  // Size

  static constexpr size_type max_size() { return capacity(); }
  static constexpr size_type capacity() { return kCapacity; }

  // All other methods are implemented on the generic-sized base class.

 private:
  friend class BasicInlineDequeImpl<value_type, size_type, kGenericSized>;

  static_assert(kCapacity <= std::numeric_limits<size_type>::max());
};

// Specialization of ``BasicInlineDequeImpl`` for trivially-destructible
// ``ValueType``. This specialization ensures that no destructor is generated.
template <typename ValueType, typename CountAndCapacityType, size_t kCapacity>
class BasicInlineDequeStorage<ValueType, CountAndCapacityType, kCapacity, true>
    : public BasicInlineDequeImpl<ValueType,
                                  CountAndCapacityType,
                                  kGenericSized> {
  // NOTE: no destructor is added, as `ValueType` is trivially-destructible.
 private:
  friend class BasicInlineDequeImpl<ValueType, CountAndCapacityType, kCapacity>;
  friend class BasicInlineDequeImpl<ValueType,
                                    CountAndCapacityType,
                                    kGenericSized>;

  using Base =
      BasicInlineDequeImpl<ValueType, CountAndCapacityType, kGenericSized>;

  constexpr BasicInlineDequeStorage() : Base(kCapacity) {}

  // The data() function is defined differently for the generic-sized and
  // known-sized specializations. This data() implementation simply returns the
  // RawStorage's data(). The generic-sized data() function casts *this to a
  // known zero-sized specialization to access this exact function.
  typename Base::pointer data() { return raw_storage_.data(); }
  typename Base::const_pointer data() const { return raw_storage_.data(); }

  // Note that this is offset and aligned the same for all possible
  // kCapacity values for the same value_type.
  RawStorage<ValueType, kCapacity> raw_storage_;
};

// Specialization of ``BasicInlineDequeImpl`` for non-trivially-destructible
// ``ValueType``. This specialization ensures that the queue is cleared
// during destruction prior to the invalidation of the `raw_storage_`.
template <typename ValueType, typename CountAndCapacityType, size_t kCapacity>
class BasicInlineDequeStorage<ValueType, CountAndCapacityType, kCapacity, false>
    : public BasicInlineDequeImpl<ValueType,
                                  CountAndCapacityType,
                                  kGenericSized> {
 public:
  ~BasicInlineDequeStorage() { Base::clear(); }

 private:
  friend class BasicInlineDequeImpl<ValueType, CountAndCapacityType, kCapacity>;
  friend class BasicInlineDequeImpl<ValueType,
                                    CountAndCapacityType,
                                    kGenericSized>;

  using Base =
      BasicInlineDequeImpl<ValueType, CountAndCapacityType, kGenericSized>;

  constexpr BasicInlineDequeStorage() : Base(kCapacity) {}

  // The data() function is defined differently for the generic-sized and
  // known-sized specializations. This data() implementation simply returns the
  // RawStorage's data(). The generic-sized data() function casts *this to a
  // known zero-sized specialization to access this exact function.
  typename Base::pointer data() { return raw_storage_.data(); }
  typename Base::const_pointer data() const { return raw_storage_.data(); }

  // Note that this is offset and aligned the same for all possible
  // kCapacity values for the same value_type.
  RawStorage<ValueType, kCapacity> raw_storage_;
};

// Defines the generic-sized BasicInlineDequeImpl<T> specialization, which
// serves as the base class for BasicInlineDequeImpl<T> of any capacity.
//
// Except for constructors and destructors, all other methods should be
// implemented on this generic-sized specialization. Destructors must
// only be written for the `BasicInlineDequeStorage` type in order to ensure
// that `raw_storage_` is still valid at the time of destruction.
//
// NOTE: this size-polymorphic base class must not be used inside of
// ``std::unique_ptr`` or ``delete``.
template <typename ValueType, typename CountAndCapacityType>
class BasicInlineDequeImpl<ValueType, CountAndCapacityType, kGenericSized>
    : public GenericDeque<
          BasicInlineDequeImpl<ValueType, CountAndCapacityType, kGenericSized>,
          ValueType,
          CountAndCapacityType> {
 private:
  using Base = GenericDeque<
      BasicInlineDequeImpl<ValueType, CountAndCapacityType, kGenericSized>,
      ValueType,
      CountAndCapacityType>;

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
  BasicInlineDequeImpl() = delete;

  BasicInlineDequeImpl& operator=(const BasicInlineDequeImpl& other) {
    Base::assign(other.begin(), other.end());
    return *this;
  }

  BasicInlineDequeImpl& operator=(BasicInlineDequeImpl&& other) noexcept {
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
  constexpr BasicInlineDequeImpl(size_type capacity) noexcept
      : Base(capacity) {}

  // Polymorphic-sized `pw::InlineDeque<T>` may not be used with `unique_ptr`
  // or `delete`. `delete` could be supported using C++20's destroying delete.
  ~BasicInlineDequeImpl() = default;

 private:
  friend Base;

  static constexpr bool kFixedCapacity = true;  // uses static allocation

  // The underlying RawStorage is not part of the generic-sized class. It is
  // provided in the derived class from which this instance was constructed. To
  // access the data, down-cast this to a known max size specialization, and
  // return the RawStorage's data, which is the same for all sizes.
  pointer data() {
    return static_cast<
               BasicInlineDequeImpl<ValueType, CountAndCapacityType, 0>*>(this)
        ->data();
  }
  const_pointer data() const {
    return static_cast<
               const BasicInlineDequeImpl<ValueType, CountAndCapacityType, 0>*>(
               this)
        ->data();
  }
};

}  // namespace containers::internal
}  // namespace pw
