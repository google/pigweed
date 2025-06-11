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
#include "pw_containers/inline_async_deque.h"
#include "pw_containers/inline_queue.h"
#include "pw_containers/internal/async_count_and_capacity.h"

namespace pw {

// Forward declaration.
template <typename ValueType, typename SizeType, size_t kCapacity>
class BasicInlineAsyncQueue;

/// Async wrapper around BasicInlineQueue.
///
/// This class mimics the structure of `BasicInlineQueue` to allow referring to
/// an `InlineAsyncQueue` without an explicit maximum size.
template <typename ValueType,
          size_t kCapacity = containers::internal::kGenericSized>
using InlineAsyncQueue = BasicInlineAsyncQueue<ValueType, uint16_t, kCapacity>;

template <typename ValueType,
          typename SizeType,
          size_t kCapacity = containers::internal::kGenericSized>
class BasicInlineAsyncQueue
    : public BasicInlineAsyncQueue<ValueType,
                                   SizeType,
                                   containers::internal::kGenericSized> {
 private:
  using Base = BasicInlineAsyncQueue<ValueType,
                                     SizeType,
                                     containers::internal::kGenericSized>;
  using Deque = BasicInlineAsyncDeque<ValueType, SizeType, kCapacity>;

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

  constexpr BasicInlineAsyncQueue() noexcept = default;

  BasicInlineAsyncQueue(size_type count, const_reference value)
      : deque_(count, value) {}

  explicit BasicInlineAsyncQueue(size_type count) : deque_(count) {}

  template <
      typename InputIterator,
      typename = containers::internal::EnableIfInputIterator<InputIterator>>
  BasicInlineAsyncQueue(InputIterator start, InputIterator finish)
      : deque_(start, finish) {}

  BasicInlineAsyncQueue(const std::initializer_list<value_type>& list)
      : deque_(list) {}

  /// Copy constructs for matching capacity.
  BasicInlineAsyncQueue(const BasicInlineAsyncQueue& other) : deque_(other) {}

  /// Copy constructs for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineAsyncQueue(
      const BasicInlineAsyncQueue<ValueType, SizeType, kOtherCapacity>& other) {
    *this = other;
  }

  /// Move constructs for matching capacity.
  BasicInlineAsyncQueue(BasicInlineAsyncQueue&& other)
      : deque_(std::move(other.deque_)) {}

  /// Move constructs for mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineAsyncQueue(
      BasicInlineAsyncQueue<ValueType, SizeType, kOtherCapacity>&& other) {
    *this = std::move(other);
  }

  BasicInlineAsyncQueue& operator=(
      const std::initializer_list<value_type>& list) {
    deque_ = std::move(list);
    return *this;
  }

  /// Copy assigns from matching capacity.
  BasicInlineAsyncQueue& operator=(const BasicInlineAsyncQueue& other) {
    deque_ = other.deque_;
    return *this;
  }

  /// Copy assigns from mismatched capacity.
  ///
  /// Note that this will result in a crash if `other.size() > capacity()`.
  template <size_t kOtherCapacity>
  BasicInlineAsyncQueue& operator=(
      const BasicInlineAsyncQueue<ValueType, SizeType, kOtherCapacity>& other) {
    deque_ = other.deque_;
    return *this;
  }

  /// Move assigns from matching capacity.
  BasicInlineAsyncQueue& operator=(BasicInlineAsyncQueue&& other) {
    deque_ = std::move(other.deque_);
    return *this;
  }

  /// Move assigns from mismatched capacity.
  ///
  /// Note that this will result in a crash if `kOtherCapacity < size()`.
  template <size_t kOtherCapacity>
  BasicInlineAsyncQueue& operator=(
      BasicInlineAsyncQueue<ValueType, SizeType, kOtherCapacity>&& other) {
    deque_ = std::move(other.deque_);
    return *this;
  }

  template <typename OtherValueType,
            typename = containers::internal::EnableIfIterable<OtherValueType>>
  BasicInlineAsyncQueue& operator=(const OtherValueType& other) {
    deque_ = Deque(other.begin(), other.end());
    return *this;
  }

  // Size

  static constexpr size_type max_size() { return capacity(); }
  static constexpr size_type capacity() { return kCapacity; }

 private:
  friend class BasicInlineAsyncQueue<ValueType,
                                     SizeType,
                                     containers::internal::kGenericSized>;

  Deque deque_;
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
class BasicInlineAsyncQueue<ValueType,
                            SizeType,
                            containers::internal::kGenericSized>
    : public containers::internal::BasicInlineQueueImpl<
          BasicInlineAsyncQueue<ValueType, SizeType>,
          BasicInlineAsyncDeque<ValueType, SizeType>> {
 private:
  using Deque = BasicInlineAsyncDeque<ValueType, SizeType>;
  using Base = containers::internal::
      BasicInlineQueueImpl<BasicInlineAsyncQueue<ValueType, SizeType>, Deque>;

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

  /// Returns `Pending` until space for `num` elements is available.
  async2::Poll<> PendAvailable(async2::Context& context, size_type num = 1) {
    return deque().PendAvailable(context, num);
  }

 private:
  template <typename, typename>
  friend class containers::internal::GenericQueue;

  template <size_t kCapacity>
  using Derived = BasicInlineAsyncQueue<value_type, size_type, kCapacity>;

  Deque& deque() { return static_cast<Derived<0>*>(this)->deque_; }
  const Deque& deque() const {
    return static_cast<const Derived<0>*>(this)->deque_;
  }
};

}  // namespace pw
