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

#include <memory>
#include <utility>

namespace pw::allocator {

// Forward declaration.
class Deallocator;

namespace internal {

/// This class simply provides a type-erased static method to deallocate the
/// memory in a unique pointer. This allows ``UniquePtr<T>`` to be declared
/// without a complete declaration of ``Deallocator``, breaking the
/// dependency cycle between ``UniquePtr<T>` and ``Allocator::MakeUnique<T>()``.
class BaseUniquePtr {
 protected:
  static void Deallocate(Deallocator* deallocator, void* ptr);
};

}  // namespace internal

/// An RAII pointer to a value of type ``T`` stored in memory provided by a
/// ``Deallocator``.
///
/// This is analogous to ``std::unique_ptr``, but includes a few differences
/// in order to support ``Deallocator`` and encourage safe usage. Most
/// notably, ``UniquePtr<T>`` cannot be constructed from a ``T*``.
template <typename T>
class UniquePtr : public internal::BaseUniquePtr {
 public:
  /// Creates an empty (``nullptr``) instance.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// ``Deallocator::MakeUnique``.
  constexpr UniquePtr() : value_(nullptr), deallocator_(nullptr) {}

  /// Creates an empty (``nullptr``) instance.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// ``Deallocator::MakeUnique``.
  constexpr UniquePtr(std::nullptr_t) : UniquePtr() {}

  /// Move-constructs a ``UniquePtr<T>`` from a ``UniquePtr<U>``.
  ///
  /// This allows not only pure move construction where ``T == U``, but also
  /// converting construction where ``T`` is a base class of ``U``, like
  /// ``UniquePtr<Base> base(deallocator.MakeUnique<Child>());``.
  template <typename U>
  UniquePtr(UniquePtr<U>&& other) noexcept
      : value_(other.value_), deallocator_(other.deallocator_) {
    static_assert(
        std::is_assignable_v<T*&, U*>,
        "Attempted to construct a UniquePtr<T> from a UniquePtr<U> where "
        "U* is not assignable to T*.");
    other.Release();
  }

  // Move-only. These are needed since the templated move-contructor and
  // move-assignment operator do not exactly match the signature of the default
  // move-contructor and move-assignment operator, and thus do not implicitly
  // delete the copy-contructor and copy-assignment operator.
  UniquePtr(const UniquePtr&) = delete;
  UniquePtr& operator=(const UniquePtr&) = delete;

  /// Move-assigns a ``UniquePtr<T>`` from a ``UniquePtr<U>``.
  ///
  /// This operation destructs and deallocates any value currently stored in
  /// ``this``.
  ///
  /// This allows not only pure move assignment where ``T == U``, but also
  /// converting assignment where ``T`` is a base class of ``U``, like
  /// ``UniquePtr<Base> base = deallocator.MakeUnique<Child>();``.
  template <typename U>
  UniquePtr& operator=(UniquePtr<U>&& other) noexcept {
    static_assert(std::is_assignable_v<T*&, U*>,
                  "Attempted to assign a UniquePtr<U> to a UniquePtr<T> where "
                  "U* is not assignable to T*.");
    Reset();
    value_ = other.value_;
    deallocator_ = other.deallocator_;
    other.Release();
    return *this;
  }

  /// Sets this ``UniquePtr`` to null, destructing and deallocating any
  /// currently-held value.
  ///
  /// After this function returns, this ``UniquePtr`` will be in an "empty"
  /// (``nullptr``) state until a new value is assigned.
  UniquePtr& operator=(std::nullptr_t) { Reset(); }

  /// Destructs and deallocates any currently-held value.
  ~UniquePtr() { Reset(); }

  /// Returns a pointer to the object that can destroy the value.
  Deallocator* deallocator() const { return deallocator_; }

  /// Releases a value from the ``UniquePtr`` without destructing or
  /// deallocating it.
  ///
  /// After this call, the object will have an "empty" (``nullptr``) value.
  T* Release() {
    T* value = value_;
    value_ = nullptr;
    deallocator_ = nullptr;
    return value;
  }

  /// Destructs and deallocates any currently-held value.
  ///
  /// After this function returns, this ``UniquePtr`` will be in an "empty"
  /// (``nullptr``) state until a new value is assigned.
  void Reset() {
    if (value_ != nullptr) {
      std::destroy_at(value_);
      internal::BaseUniquePtr::Deallocate(deallocator_, value_);
      Release();
    }
  }

  /// ``operator bool`` is not provided in order to ensure that there is no
  /// confusion surrounding ``if (foo)`` vs. ``if (*foo)``.
  ///
  /// ``nullptr`` checking should instead use ``if (foo == nullptr)``.
  explicit operator bool() const = delete;

  /// Returns whether this ``UniquePtr`` is in an "empty" (``nullptr``) state.
  bool operator==(std::nullptr_t) const { return value_ == nullptr; }

  /// Returns whether this ``UniquePtr`` is not in an "empty" (``nullptr``)
  /// state.
  bool operator!=(std::nullptr_t) const { return value_ != nullptr; }

  /// Returns the underlying (possibly null) pointer.
  T* get() { return value_; }
  /// Returns the underlying (possibly null) pointer.
  const T* get() const { return value_; }

  /// Permits accesses to members of ``T`` via ``my_unique_ptr->Member``.
  ///
  /// The behavior of this operation is undefined if this ``UniquePtr`` is in an
  /// "empty" (``nullptr``) state.
  T* operator->() { return value_; }
  const T* operator->() const { return value_; }

  /// Returns a reference to any underlying value.
  ///
  /// The behavior of this operation is undefined if this ``UniquePtr`` is in an
  /// "empty" (``nullptr``) state.
  T& operator*() { return *value_; }
  const T& operator*() const { return *value_; }

 private:
  /// A pointer to the contained value.
  T* value_;

  /// The ``deallocator_`` which provided the memory for  ``value_``.
  /// This must be tracked in order to deallocate the memory upon destruction.
  Deallocator* deallocator_;

  /// Allow converting move constructor and assignment to access fields of
  /// this class.
  ///
  /// Without this, ``UniquePtr<U>`` would not be able to access fields of
  /// ``UniquePtr<T>``.
  template <typename U>
  friend class UniquePtr;

  class PrivateConstructorType {};
  static constexpr PrivateConstructorType kPrivateConstructor{};

 public:
  /// Private constructor that is public only for use with `emplace` and
  /// other in-place construction functions.
  ///
  /// Constructs a ``UniquePtr`` from an already-allocated value.
  ///
  /// NOTE: Instances of this type are most commonly constructed using
  /// ``Deallocator::MakeUnique``.
  UniquePtr(PrivateConstructorType, T* value, Deallocator* deallocator)
      : value_(value), deallocator_(deallocator) {}

  // Allow construction with ``kPrivateConstructor`` to the implementation
  // of ``MakeUnique``.
  friend class Deallocator;
};

}  // namespace pw::allocator
