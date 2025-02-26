// Copyright 2024 The Pigweed Authors
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
#include <type_traits>
#include <utility>

#include "pw_allocator/config.h"
#include "pw_allocator/internal/managed_ptr.h"
#include "pw_preprocessor/compiler.h"

namespace pw {

/// A `std::unique_ptr<T>`-like type that integrates with `pw::Deallocator`.
///
/// This is a RAII smart pointer that deallocates any memory it points to when
/// it goes out of scope.
///
/// Its most notable difference from `std::unique_ptr<T>` is that it cannot be
/// constructed from a `T*`. Use `Allocator::MakeUnique<T>(...)` instead.
///
/// @tparam   T   The type being pointed to. This may be an array type, e.g.
///           `pw::UniquePtr<T[]>`.
///
/// TODO(b/399441816): This class should be marked final, but at least one
/// downstream has extended it. Resolve and mark final.
template <typename T>
class UniquePtr : public ::pw::allocator::internal::ManagedPtr<T> {
 private:
  using Base = ::pw::allocator::internal::ManagedPtr<T>;
  using Empty = ::pw::allocator::internal::Empty;

 public:
  using pointer = typename Base::element_type*;
  using element_type = typename Base::element_type;

  /// Creates an empty (`nullptr`) instance.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// `Allocator::MakeUnique`.
  constexpr UniquePtr() noexcept {
    if constexpr (std::is_array_v<T>) {
      size_ = 0;
    }
  }

  /// Creates an empty (`nullptr`) instance.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// `Allocator::MakeUnique`.
  constexpr UniquePtr(std::nullptr_t) noexcept : UniquePtr() {}

  /// Move-constructs a `UniquePtr<T>` from a `UniquePtr<U>`.
  ///
  /// This allows not only pure move construction where `T == U`, but also
  /// converting construction where `T` is a base class of `U`, like
  /// `UniquePtr<Base> base(deallocator.MakeUnique<Child>());`.
  template <typename U>
  UniquePtr(UniquePtr<U>&& other) noexcept {
    *this = std::move(other);
  }

  /// Frees any currently-held value.
  ~UniquePtr() { Reset(); }

  /// Move-assigns a `UniquePtr<T>` from a `UniquePtr<U>`.
  ///
  /// This operation frees the value currently stored in `this`.
  ///
  /// This allows not only pure move assignment where `T == U`, but also
  /// converting assignment where `T` is a base class of `U`.
  template <typename U>
  UniquePtr& operator=(UniquePtr<U>&& other) noexcept;

  /// Sets this `UniquePtr` to null, freeing any currently-held value.
  ///
  /// After this function returns, this `UniquePtr` will be in an "empty"
  /// (`nullptr`) state until a new value is assigned.
  UniquePtr& operator=(std::nullptr_t) noexcept;

  /// Returns the number of elements allocated.
  ///
  /// This will fail to compile if it is called on a non-array type UniquePtr.
  size_t size() const {
    static_assert(std::is_array_v<T>,
                  "size() cannot be called with a non-array type");
    return size_;
  }

  /// Returns a pointer to the object that can destroy the value.
  Deallocator* deallocator() const { return deallocator_; }

  /// Releases a value from the `UniquePtr` without destructing or
  /// deallocating it.
  ///
  /// After this call, the object will have an "empty" (`nullptr`) value.
  element_type* Release() noexcept;

  /// Destroys and deallocates any currently-held value.
  ///
  /// After this function returns, this `UniquePtr` will be in an "empty"
  /// (`nullptr`) state until a new value is assigned.
  void Reset() noexcept;

  /// Swaps the managed pointer and deallocator of this and another object.
  void Swap(UniquePtr& other);

 private:
  // Allow construction with to the implementation of `MakeUnique`.
  friend class Deallocator;

  // Allow UniquePtr<T> to access UniquePtr<U> and vice versa.
  template <typename>
  friend class UniquePtr;

  /// Private constructor that is public only for use with `emplace` and
  /// other in-place construction functions.
  ///
  /// Constructs a `UniquePtr` from an already-allocated value.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// `Deallocator::MakeUnique`.
  UniquePtr(element_type* value, Deallocator* deallocator)
      : Base(value), deallocator_(deallocator) {}

  /// Private constructor that is public only for use with `emplace` and
  /// other in-place construction functions.
  ///
  /// Constructs a `UniquePtr` from an already-allocated value and size.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// `Deallocator::MakeUnique`.
  UniquePtr(element_type* value, size_t size, Deallocator* deallocator)
      : Base(value), size_(size), deallocator_(deallocator) {}

  /// Copies details from another object without releasing it.
  template <typename U>
  void CopyFrom(const UniquePtr<U>& other);

  /// The number of elements allocated. This will not be present in the case
  /// where T is not an array type as this will be the empty struct type
  /// optimized out.
  PW_NO_UNIQUE_ADDRESS
  std::conditional_t<std::is_array_v<T>, size_t, Empty> size_;

  /// The `deallocator_` which can reclaim the memory for  `value_`.
  /// This must be tracked in order to deallocate the memory upon destruction.
  Deallocator* deallocator_ = nullptr;
};

namespace allocator {

// Alias for module consumers using the older name for the above type.
template <typename T>
using UniquePtr = PW_ALLOCATOR_DEPRECATED ::pw::UniquePtr<T>;

}  // namespace allocator

// Template method implementations.

template <typename T>
template <typename U>
UniquePtr<T>& UniquePtr<T>::operator=(UniquePtr<U>&& other) noexcept {
  Reset();
  CopyFrom(other);
  other.Release();
  return *this;
}

template <typename T>
UniquePtr<T>& UniquePtr<T>::operator=(std::nullptr_t) noexcept {
  Reset();
  return *this;
}

template <typename T>
typename UniquePtr<T>::element_type* UniquePtr<T>::Release() noexcept {
  element_type* value = Base::Release();
  deallocator_ = nullptr;
  return value;
}

template <typename T>
void UniquePtr<T>::Reset() noexcept {
  if (*this == nullptr) {
    return;
  }
  if (!Base::HasCapability(deallocator_, allocator::kSkipsDestroy)) {
    if constexpr (std::is_array_v<T>) {
      Base::Destroy(size_);
    } else {
      Base::Destroy();
    }
  }
  Deallocator* deallocator = deallocator_;
  Base::Deallocate(deallocator, Release());
}

template <typename T>
void UniquePtr<T>::Swap(UniquePtr<T>& other) {
  Base::Swap(other);
  if constexpr (std::is_array_v<T>) {
    std::swap(size_, other.size_);
  }
  std::swap(deallocator_, other.deallocator_);
}

template <typename T>
template <typename U>
void UniquePtr<T>::CopyFrom(const UniquePtr<U>& other) {
  Base::CopyFrom(other);
  size_ = other.size_;
  deallocator_ = other.deallocator_;
}

}  // namespace pw
