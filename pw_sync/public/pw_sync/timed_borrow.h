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

#include <optional>
#include <type_traits>

#include "pw_chrono/system_clock.h"
#include "pw_sync/borrow.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/lock_traits.h"
#include "pw_sync/virtual_basic_lockable.h"

namespace pw::sync {

/// @module{pw_sync}

/// `TimedBorrowable` extends `Borrowable` with additional methods to borrow
/// an object guarded by a lock that implements the @em TimedLockable C++ named
/// requirement.
template <typename GuardedType,
          typename LockType = pw::sync::VirtualBasicLockable>
class TimedBorrowable : public Borrowable<GuardedType, LockType> {
 private:
  using Base = Borrowable<GuardedType, LockType>;

  static_assert(is_lockable_for_v<LockType, chrono::SystemClock::duration>);
  static_assert(is_lockable_until_v<LockType, chrono::SystemClock::time_point>);

 public:
  constexpr TimedBorrowable(GuardedType& object, LockType& lock) noexcept
      : Base(object, lock) {}

  template <typename U>
  constexpr TimedBorrowable(const Borrowable<U, LockType>& other)
      : Base(other) {}

  /// Tries to borrow the object. Blocks until the specified timeout has elapsed
  /// or the object has been borrowed, whichever comes first.
  ///
  /// @returns `BorrowedPointer` on success, otherwise `std::nullopt` (nothing).
  std::optional<BorrowedPointer<GuardedType, LockType>> try_acquire_for(
      chrono::SystemClock::duration timeout) const PW_NO_LOCK_SAFETY_ANALYSIS {
    if (!Base::lock_->try_lock_for(timeout)) {
      return std::nullopt;
    }
    return Base::Borrow();
  }

  /// Tries to borrow the object. Blocks until the specified deadline has passed
  /// or the object has been borrowed, whichever comes first.
  /// @returns `BorrowedPointer` on success, otherwise `std::nullopt` (nothing).
  std::optional<BorrowedPointer<GuardedType, LockType>> try_acquire_until(
      chrono::SystemClock::time_point deadline) const
      PW_NO_LOCK_SAFETY_ANALYSIS {
    if (!Base::lock_->try_lock_until(deadline)) {
      return std::nullopt;
    }
    return Base::Borrow();
  }
};

/// @}

}  // namespace pw::sync
