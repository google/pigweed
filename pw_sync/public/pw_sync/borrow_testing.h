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

#include "pw_sync/borrow.h"
#include "pw_sync/lock_traits.h"
#include "pw_unit_test/framework.h"

namespace pw::sync::test {

// Test fixtures.

/// Simple struct that wraps a value.
class BaseObj {
 public:
  static constexpr int kInitialValue = 24;

  virtual ~BaseObj() = default;

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

 protected:
  BaseObj(int value) : value_(value) {}

 private:
  int value_ = kInitialValue;
};

/// Simple struct that derives from `BaseObj` and wraps a value.
class Derived : public BaseObj {
 public:
  static constexpr int kInitialValue = 42;

  Derived() : BaseObj(kInitialValue) {}
  explicit Derived(int value) : BaseObj(value) {}
};

/// Checks if a type has a method `locked()`.
///
/// Several fake locks are used in testing that simply update a bool instead of
/// actually locking. The lock state for these types can be accessed using
/// `locked()`.
///
/// @{
template <typename LockType, typename = void>
struct has_locked : std::false_type {};

template <typename LockType>
struct has_locked<LockType,
                  std::void_t<decltype(std::declval<LockType>().locked())>>
    : std::true_type {};
/// @}

/// Fake clock for use with non-timed locks.
///
/// This clock is guaranteed to fail `is_lockable_until<LockType, ClockType>`
/// and as such is suitable to make the `TestTryAcquireUntilSuccess` and
/// `TestTryAcquireUntilFailure` tests pass trivially for lock types that do not
/// meet C++'s \em TimedLockable named requirement for any clock.
struct NoClock {
  using duration = void;
  using time_point = void;
};

/// External locking test fixture.
///
/// This type provides a set of unit tests for testing borrowing objects that
/// are protected by a given lockable type.
///
/// @tparam   LockType        Lock type to use to protect access to borrowed
///                           objects.
/// @tparam   BorrowableType  pw::sync::Borrowable or a subtype.
template <typename LockType,
          typename BorrowableType = Borrowable<Derived, LockType>>
class BorrowTest : public ::testing::Test {
 protected:
  BorrowTest() : ::testing::Test(), borrowable_(derived_, lock_) {}

  void TestAcquire() {
    {
      BorrowedPointer<Derived, LockType> borrowed = borrowable_.acquire();
      CheckLocked(true);  // Ensure the lock is held.
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
      borrowed->set_value(13);
    }
    CheckLocked(false);  // Ensure the lock is released.
    EXPECT_EQ(derived_.value(), 13);
  }

  void TestConstAcquire() {
    const Borrowable<Derived, LockType> const_borrowable(borrowable_);
    {
      BorrowedPointer<Derived, LockType> borrowed = const_borrowable.acquire();
      CheckLocked(true);  // Ensure the lock is held.
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
      borrowed->set_value(13);
    }
    CheckLocked(false);  // Ensure the lock is released.
    EXPECT_EQ(derived_.value(), 13);
  }

  void TestRepeatedAcquire() {
    {
      BorrowedPointer<Derived, LockType> borrowed = borrowable_.acquire();
      CheckLocked(true);  // Ensure the lock is held.
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
      borrowed->set_value(13);
    }
    CheckLocked(false);  // Ensure the lock is released.
    {
      BorrowedPointer<Derived, LockType> borrowed = borrowable_.acquire();
      CheckLocked(true);  // Ensure the lock is held.
      EXPECT_EQ(borrowed->value(), 13);
    }
    CheckLocked(false);  // Ensure the lock is released.
  }

  void TestMoveable() {
    Borrowable<Derived, LockType> moved = std::move(borrowable_);
    {
      BorrowedPointer<Derived, LockType> borrowed = moved.acquire();
      CheckLocked(true);  // Ensure the lock is held.
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
      borrowed->set_value(13);
    }
    CheckLocked(false);  // Ensure the lock is released.
  }

  void TestCopyable() {
    const Borrowable<Derived, LockType>& intermediate = borrowable_;
    Borrowable<Derived, LockType> copied(intermediate);
    {
      BorrowedPointer<Derived, LockType> borrowed = copied.acquire();
      CheckLocked(true);  // Ensure the lock is held.
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
      borrowed->set_value(13);
    }
    CheckLocked(false);  // Ensure the lock is released.
    EXPECT_EQ(derived_.value(), 13);
  }

  void TestCopyableCovariant() {
    const Borrowable<Derived, LockType>& intermediate = borrowable_;
    Borrowable<BaseObj, LockType> copied_base(intermediate);
    {
      BorrowedPointer<BaseObj, LockType> borrowed = copied_base.acquire();
      CheckLocked(true);  // Ensure the lock is held.
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
      borrowed->set_value(13);
    }
    CheckLocked(false);  // Ensure the lock is released.
    EXPECT_EQ(derived_.value(), 13);
  }

  // The remaining unit tests are only valid if certain lock traits are
  // satisfied by the given `LockType`. Since the class is templated on the lock
  // type, the "constexpr-else" clause is only evaluated if the method is called
  // with an invalid lock type. this will result in a build faliure indicating
  // the unmet trait.

  void TestTryAcquireSuccess() {
    if constexpr (is_lockable_v<LockType>) {
      std::optional<BorrowedPointer<Derived, LockType>> maybe_borrowed =
          borrowable_.try_acquire();
      ASSERT_TRUE(maybe_borrowed.has_value());
      CheckLocked(true);  // Ensure the lock is held.
      auto& borrowed = *maybe_borrowed;
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
    } else {
      static_assert(is_lockable_v<LockType>);
    }
    CheckLocked(false);  // Ensure the lock is released.
  }

  void TestTryAcquireFailure() {
    Lock();
    if constexpr (is_lockable_v<LockType>) {
      std::optional<BorrowedPointer<Derived, LockType>> maybe_borrowed =
          borrowable_.try_acquire();
      EXPECT_FALSE(maybe_borrowed.has_value());
    } else {
      static_assert(is_lockable_v<LockType>);
    }
    Unlock();
  }

 private:
  // Allow the derived TimedBorrowTest access BorrowTest's internals.
  template <typename>
  friend class TimedBorrowTest;

  /// Checks if a lock's state matches the expected state.
  ///
  /// This method can check fake locks used for testing as well as locks that
  /// meet C++'s \em Lockable named requirement. This method is a no-op for lock
  /// types that only meet \em BasicLockable.
  ///
  /// **Important Change:** Previously, this method attempted to use
  /// `try_lock()` to check the state of lock types that satisfied \em Lockable
  /// but not
  /// \em has_locked. However, due to limitations in some underlying RTOS
  /// implementations (such as those based on `kal_take_mutex_try`), calling
  /// `try_lock()` from a thread that already holds the lock can lead to
  /// fatal errors. To avoid this issue, the logic using `try_lock()` has been
  /// removed. As a result, for lock types that only satisfy \em BasicLockable
  /// but not \em has_locked, the `CheckLocked` method will no longer perform
  /// any checks.
  ///
  /// @param[in]  expected  Indicates if the lock is expected to be locked.
  void CheckLocked(bool expected) const {
    if constexpr (has_locked<LockType>::value) {
      EXPECT_EQ(lock_.locked(), expected);
    }
  }

  /// Explicitly locks the lock.
  ///
  /// This can be used in tests that are expected to fail when trying to acquire
  /// the lock.
  ///
  /// The caller must subsequently call `Unlock`.
  void Lock() const PW_EXCLUSIVE_LOCK_FUNCTION(lock_) {
    CheckLocked(false);
    lock_.lock();
    CheckLocked(true);
  }

  /// Explicitly unlocks the lock.
  ///
  /// This can be used in tests that are expected to fail when trying to acquire
  /// the lock.
  ///
  /// The caller must previously have called `Lock`.
  void Unlock() const PW_UNLOCK_FUNCTION(lock_) {
    CheckLocked(true);
    lock_.unlock();
    CheckLocked(false);
  }

  mutable LockType lock_;
  Derived derived_;
  BorrowableType borrowable_;
};

}  // namespace pw::sync::test
