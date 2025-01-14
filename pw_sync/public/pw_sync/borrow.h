// Copyright 2021 The Pigweed Authors
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

#include <chrono>
#include <optional>
#include <type_traits>

#include "pw_assert/assert.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/lock_traits.h"
#include "pw_sync/virtual_basic_lockable.h"

namespace pw::sync {

/// The `BorrowedPointer` is an RAII handle which wraps a pointer to a borrowed
/// object along with a held lock which is guarding the object. When destroyed,
/// the lock is released.
template <typename GuardedType,
          typename LockType = pw::sync::VirtualBasicLockable>
class BorrowedPointer {
 public:
  /// Release the lock on destruction.
  ~BorrowedPointer() {
    if (lock_ != nullptr) {
      lock_->unlock();
    }
  }

  /// Move-constructs a ``BorrowedPointer<T>`` from a ``BorrowedPointer<U>``.
  ///
  /// This allows not only pure move construction where
  /// ``GuardedType == G`` and ``Lock == L``, but also
  /// converting construction where ``GuardedType`` is a base class of
  /// ``OtherType`` and ``Lock`` is a base class of ``OtherLock``, like
  /// ``BorrowedPointer<Base> base_ptr(derived_borrowable.acquire());`
  ///
  /// @b Postcondition: The other BorrowedPointer is no longer valid and will
  ///     assert if the GuardedType is accessed.
  template <typename G, typename L>
  BorrowedPointer(BorrowedPointer<G, L>&& other)
      : lock_(other.lock_), object_(other.object_) {
    static_assert(
        std::is_assignable_v<GuardedType*&, G*>,
        "Attempted to construct a BorrowedPointer from another whose "
        "GuardedType* is not assignable to this object's GuardedType*.");
    static_assert(std::is_assignable_v<LockType*&, L*>,
                  "Attempted to construct a BorrowedPointer from another whose "
                  "LockType* is not assignable to this object's Lock*.");
    other.lock_ = nullptr;
    other.object_ = nullptr;
  }

  /// Move-assigns a ``BorrowedPointer<T>`` from a ``BorrowedPointer<U>``.
  ///
  /// This allows not only pure move construction where
  /// ``GuardedType == OtherType`` and ``Lock == OtherLock``, but also
  /// converting construction where ``GuardedType`` is a base class of
  /// ``OtherType`` and ``Lock`` is a base class of ``OtherLock``, like
  /// ``BorrowedPointer<Base> base_ptr = derived_borrowable.acquire();`
  ///
  /// @b Postcondition: The other BorrowedPointer is no longer valid and will
  ///     assert if the GuardedType is accessed.
  template <typename G, typename L>
  BorrowedPointer& operator=(BorrowedPointer<G, L>&& other) {
    static_assert(
        std::is_assignable_v<GuardedType*&, G*>,
        "Attempted to construct a BorrowedPointer from another whose "
        "GuardedType* is not assignable to this object's GuardedType*.");
    static_assert(std::is_assignable_v<LockType*&, L*>,
                  "Attempted to construct a BorrowedPointer from another whose "
                  "LockType* is not assignable to this object's Lock*.");
    lock_ = other.lock_;
    object_ = other.object_;
    other.lock_ = nullptr;
    other.object_ = nullptr;
    return *this;
  }
  BorrowedPointer(const BorrowedPointer&) = delete;
  BorrowedPointer& operator=(const BorrowedPointer&) = delete;

  /// Provides access to the borrowed object's members.
  GuardedType* operator->() {
    PW_ASSERT(object_ != nullptr);  // Ensure this isn't a stale moved instance.
    return object_;
  }

  /// Const overload
  const GuardedType* operator->() const {
    PW_ASSERT(object_ != nullptr);  // Ensure this isn't a stale moved instance.
    return object_;
  }

  /// Provides access to the borrowed object directly.
  ///
  /// @rst
  /// .. note::
  ///    The member of pointer member access operator, ``operator->()``, is
  ///    recommended over this API as this is prone to leaking references.
  ///    However, this is sometimes necessary.
  ///
  /// .. warning:
  ///    Be careful not to leak references to the borrowed object!
  /// @endrst
  GuardedType& operator*() {
    PW_ASSERT(object_ != nullptr);  // Ensure this isn't a stale moved instance.
    return *object_;
  }

  /// Const overload
  const GuardedType& operator*() const {
    PW_ASSERT(object_ != nullptr);  // Ensure this isn't a stale moved instance.
    return *object_;
  }

 private:
  /// Allow BorrowedPointer creation inside of Borrowable's acquire methods.
  template <typename, typename>
  friend class Borrowable;

  constexpr BorrowedPointer(LockType& lock, GuardedType& object)
      : lock_(&lock), object_(&object) {}

  LockType* lock_;
  GuardedType* object_;

  /// Allow converting move constructor and assignment to access fields of
  /// this class.
  template <typename, typename>
  friend class BorrowedPointer;
};

/// The `Borrowable` is a helper construct that enables callers to borrow an
/// object which is guarded by a lock.
///
/// Users who need access to the guarded object can ask to acquire a
/// `BorrowedPointer` which permits access while the lock is held.
///
/// Thread-safety analysis is not supported for this class, as the
/// `BorrowedPointer`s it creates conditionally releases the lock. See also
/// https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#no-conditionally-held-locks
///
/// This class is compatible with locks which comply with \em BasicLockable C++
/// named requirement. A `try_acquire` method is conditionally available if the
/// lock also meets the \em Lockable requirement. This class is further extended
/// by `TimedBorrowable` for locks that meet the \em TimedLockable requirement.
///
/// `Borrowable<T>` is covariant with respect to `T`, so that `Borrowable<U>`
/// can be converted to `Borrowable<T>`, if `U` is a subclass of `T`.
///
/// `Borrowable` has pointer-like semantics and should be passed by value.
template <typename GuardedType,
          typename LockType = pw::sync::VirtualBasicLockable>
class Borrowable {
 public:
  static_assert(is_basic_lockable_v<LockType>,
                "lock type must satisfy BasicLockable");

  constexpr Borrowable(GuardedType& object, LockType& lock) noexcept
      : lock_(&lock), object_(&object) {}

  template <typename U>
  constexpr Borrowable(const Borrowable<U, LockType>& other)
      : lock_(other.lock_), object_(other.object_) {}

  Borrowable(const Borrowable&) = default;
  Borrowable& operator=(const Borrowable&) = default;
  Borrowable(Borrowable&& other) = default;
  Borrowable& operator=(Borrowable&& other) = default;

  /// Blocks indefinitely until the object can be borrowed. Failures are fatal.
  BorrowedPointer<GuardedType, LockType> acquire() const
      PW_NO_LOCK_SAFETY_ANALYSIS {
    lock_->lock();
    return Borrow();
  }

  /// Tries to borrow the object in a non-blocking manner. Returns a
  /// BorrowedPointer on success, otherwise `std::nullopt` (nothing).
  template <int&... ExplicitArgumentBarrier,
            typename T = LockType,
            typename = std::enable_if_t<is_lockable_v<T>>>
  std::optional<BorrowedPointer<GuardedType, LockType>> try_acquire() const
      PW_NO_LOCK_SAFETY_ANALYSIS {
    if (!lock_->try_lock()) {
      return std::nullopt;
    }
    return Borrow();
  }

 private:
  // Befriend all template instantiations of this class and the Timed subtype.
  template <typename, typename>
  friend class Borrowable;

  template <typename, typename>
  friend class TimedBorrowable;

  BorrowedPointer<GuardedType, LockType> Borrow() const {
    return BorrowedPointer<GuardedType, LockType>(*lock_, *object_);
  }

  LockType* lock_;
  GuardedType* object_;
};

}  // namespace pw::sync
