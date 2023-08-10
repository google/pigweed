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

/// This file contains tests that can be used to verify a lock type can be
/// used in `pw::sync::Borrowable` to borrow types that use external locking.
///
/// Locks must at least meet C++'s \em BasicLockable named requirement. Tests
/// should be added using the `ADD_BORROWABLE_...` macros from this file.
///
/// * If a lock is not \em TimedLockable, use `ADD_BORROWABLE_LOCK_TESTS`, e.g.
///   `ADD_BORROWABLE_LOCK_TESTS(MyLock);`.
///
/// * If a lock is \em TimedLockable, use `ADD_BORROWABLE_TIMED_LOCK_TESTS` and
///   provide the appropriate clock, e.g.
///   `ADD_BORROWABLE_TIMED_LOCK_TESTS(MyLock, pw::chrono::SystemClock);`
///
/// * If the default test suite name is not suitable, use the `..._NAMED_TESTS`
///   variants, e.g.
///   `ADD_BORROWABLE_LOCK_NAMED_TESTS(MyTestSuite, pw::my_module::Lock);`.

#include "gtest/gtest.h"
#include "pw_sync/borrow.h"
#include "pw_sync/lock_traits.h"

namespace pw::sync {

// Test fixtures.

/// Simple struct that wraps a value.
struct Base {
  static constexpr int kInitialValue = 24;
  int base_value = kInitialValue;
};

/// Simple struct that derives from `Base` and wraps a value.
struct Derived : public Base {
  static constexpr int kInitialValue = 42;
  int value = kInitialValue;
};

/// Checks if a type has a method `locked()`.
///
/// Several fake locks are used in testing that simply update a bool instead of
/// actually locking. The lock state for these types can be accessed using
/// `locked()`.
///
/// @{
template <typename Lock, typename = void>
struct has_locked : std::false_type {};

template <typename Lock>
struct has_locked<Lock, std::void_t<decltype(std::declval<Lock>().locked())>>
    : std::true_type {};
/// @}

/// Checks if a lock's state matches the expected state.
///
/// This method can check fake locks used for testing as well as locks that
/// meet C++'s \em Lockable named requirement. This method is a no-op for lock
/// types that only meet \em BasicLockable.
///
/// @param[in]  lock      The lock to check.
/// @param[in]  expected  Indicates if the lock is expected to be locked.
template <typename Lock>
void CheckLocked(Lock& lock, bool expected) {
  if constexpr (has_locked<Lock>::value) {
    EXPECT_EQ(lock.locked(), expected);
  } else if constexpr (is_lockable_v<Lock>) {
    bool locked = !lock.try_lock();
    EXPECT_EQ(locked, expected);
    if (!locked) {
      lock.unlock();
    }
  }
}

template <typename Lock>
void TestAcquire() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  {
    BorrowedPointer<Derived, Lock> borrowed = borrowable.acquire();
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(borrowed->value, Derived::kInitialValue);
    borrowed->value = 13;
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
  EXPECT_EQ(derived.value, 13);
}

template <typename Lock>
void TestConstAcquire() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  const Borrowable<Derived, Lock> const_borrowable(borrowable);
  {
    BorrowedPointer<Derived, Lock> borrowed = const_borrowable.acquire();
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(borrowed->value, Derived::kInitialValue);
    borrowed->value = 13;
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
  EXPECT_EQ(derived.value, 13);
}

template <typename Lock>
void TestRepeatedAcquire() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  {
    BorrowedPointer<Derived, Lock> borrowed = borrowable.acquire();
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(borrowed->value, Derived::kInitialValue);
    borrowed->value = 13;
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
  {
    BorrowedPointer<Derived, Lock> borrowed = borrowable.acquire();
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(borrowed->value, 13);
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
}

template <typename Lock>
void TestMoveable() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  Borrowable<Derived, Lock> moved = std::move(borrowable);
  {
    BorrowedPointer<Derived, Lock> borrowed = moved.acquire();
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(borrowed->value, Derived::kInitialValue);
    borrowed->value = 13;
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
}

template <typename Lock>
void TestCopyable() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  const Borrowable<Derived, Lock>& intermediate = borrowable;
  Borrowable<Derived, Lock> copied(intermediate);
  {
    BorrowedPointer<Derived, Lock> borrowed = copied.acquire();
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(borrowed->value, Derived::kInitialValue);
    borrowed->value = 13;
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
  EXPECT_EQ(derived.value, 13);
}

template <typename Lock>
void TestCopyableCovariant() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  const Borrowable<Derived, Lock>& intermediate = borrowable;
  Borrowable<Base, Lock> copied_base(intermediate);
  {
    BorrowedPointer<Base, Lock> borrowed = copied_base.acquire();
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(borrowed->base_value, Base::kInitialValue);
    borrowed->base_value = 13;
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
  EXPECT_EQ(derived.base_value, 13);
}

template <typename Lock>
void TestTryAcquireSuccess() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  if constexpr (is_lockable_v<Lock>) {
    std::optional<BorrowedPointer<Derived, Lock>> maybe_borrowed =
        borrowable.try_acquire();
    ASSERT_TRUE(maybe_borrowed.has_value());
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(maybe_borrowed.value()->value, Derived::kInitialValue);
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
}

template <typename Lock>
void TestTryAcquireFailure() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  lock.lock();
  CheckLocked(lock, true);  // Ensure the lock is held.
  if constexpr (is_lockable_v<Lock>) {
    std::optional<BorrowedPointer<Derived, Lock>> maybe_borrowed =
        borrowable.try_acquire();
    EXPECT_FALSE(maybe_borrowed.has_value());
  }
  CheckLocked(lock, true);  // Ensure the lock is held.
  lock.unlock();
}

template <typename Lock>
void TestTryAcquireForSuccess() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  if constexpr (is_lockable_for_v<Lock, decltype(std::chrono::seconds(0))>) {
    std::optional<BorrowedPointer<Derived, Lock>> maybe_borrowed =
        borrowable.try_acquire_for(std::chrono::seconds(0));
    ASSERT_TRUE(maybe_borrowed.has_value());
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(maybe_borrowed.value()->value, Derived::kInitialValue);
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
}

template <typename Lock>
void TestTryAcquireForFailure() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  lock.lock();
  CheckLocked(lock, true);  // Ensure the lock is held.
  if constexpr (is_lockable_for_v<Lock, decltype(std::chrono::seconds(0))>) {
    std::optional<BorrowedPointer<Derived, Lock>> maybe_borrowed =
        borrowable.try_acquire_for(std::chrono::seconds(0));
    EXPECT_FALSE(maybe_borrowed.has_value());
  }
  CheckLocked(lock, true);  // Ensure the lock is held.
  lock.unlock();
}

/// Fake clock for use with non-timed locks.
///
/// This clock is guaranteed to fail `is_lockable_until<Lock, Clock>` and as
/// such is suitable to make the `TestTryAcquireUntilSuccess` and
/// `TestTryAcquireUntilFailure` tests pass trivially for lock types that do not
/// meet C++'s \em TimedLockable named requirement for any clock.
struct NoClock {
  using duration = void;
  using time_point = void;
};

template <typename Lock, typename Clock = NoClock>
void TestTryAcquireUntilSuccess() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  if constexpr (is_lockable_until_v<Lock, Clock>) {
    std::optional<BorrowedPointer<Derived, Lock>> maybe_borrowed =
        borrowable.try_acquire_until(Clock::time_point());
    ASSERT_TRUE(maybe_borrowed.has_value());
    CheckLocked(lock, true);  // Ensure the lock is held.
    EXPECT_EQ(maybe_borrowed.value()->value, Derived::kInitialValue);
  }
  CheckLocked(lock, false);  // Ensure the lock is released.
}

template <typename Lock, typename Clock = NoClock>
void TestTryAcquireUntilFailure() {
  Lock lock;
  Derived derived;
  Borrowable<Derived, Lock> borrowable(derived, lock);
  lock.lock();
  CheckLocked(lock, true);  // Ensure the lock is held.
  if constexpr (is_lockable_until_v<Lock, Clock>) {
    std::optional<BorrowedPointer<Derived, Lock>> maybe_borrowed =
        borrowable.try_acquire_until(Clock::time_point());
    EXPECT_FALSE(maybe_borrowed.has_value());
  }
  CheckLocked(lock, true);  // Ensure the lock is held.
  lock.unlock();
}

/// Register borrowable non-timed lock tests.
#define PW_SYNC_ADD_BORROWABLE_LOCK_TESTS(lock) \
  PW_SYNC_ADD_BORROWABLE_TIMED_LOCK_TESTS(lock, NoClock)

/// Register borrowable non-timed lock tests in a named test suite.
#define PW_SYNC_ADD_BORROWABLE_LOCK_NAMED_TESTS(name, lock) \
  PW_SYNC_ADD_BORROWABLE_TIMED_LOCK_NAMED_TESTS(name, lock, NoClock)

/// Register all borrowable lock tests.
#define PW_SYNC_ADD_BORROWABLE_TIMED_LOCK_TESTS(lock, clock) \
  PW_SYNC_ADD_BORROWABLE_TIMED_LOCK_NAMED_TESTS(             \
      Borrowable##lock##Test, lock, clock)

/// Register all borrowable lock tests in a named test suite.
#define PW_SYNC_ADD_BORROWABLE_TIMED_LOCK_NAMED_TESTS(name, lock, clock) \
  TEST(name, Acquire) { TestAcquire<lock>(); }                           \
  TEST(name, ConstAcquire) { TestConstAcquire<lock>(); }                 \
  TEST(name, RepeatedAcquire) { TestRepeatedAcquire<lock>(); }           \
  TEST(name, Moveable) { TestMoveable<lock>(); }                         \
  TEST(name, Copyable) { TestCopyable<lock>(); }                         \
  TEST(name, CopyableCovariant) { TestCopyableCovariant<lock>(); }       \
  TEST(name, TryAcquireForSuccess) { TestTryAcquireForSuccess<lock>(); } \
  TEST(name, TryAcquireForFailure) { TestTryAcquireForFailure<lock>(); } \
  TEST(name, TryAcquireUntilSuccess) {                                   \
    TestTryAcquireUntilSuccess<lock, clock>();                           \
  }                                                                      \
  TEST(name, TryAcquireUntilFailure) {                                   \
    TestTryAcquireUntilFailure<lock, clock>();                           \
  }                                                                      \
  static_assert(true, "trailing semicolon")

}  // namespace pw::sync
