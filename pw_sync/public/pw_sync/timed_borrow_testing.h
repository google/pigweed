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

#include "pw_sync/borrow_testing.h"
#include "pw_sync/timed_borrow.h"
#include "pw_unit_test/framework.h"

namespace pw::sync::test {

template <typename LockType>
class TimedBorrowTest
    : public BorrowTest<LockType, TimedBorrowable<Derived, LockType>> {
 private:
  using Base = BorrowTest<LockType, TimedBorrowable<Derived, LockType>>;

 protected:
  void TestTryAcquireForSuccess(chrono::SystemClock::duration timeout) {
    {
      std::optional<BorrowedPointer<Derived, LockType>> maybe_borrowed =
          Base::borrowable_.try_acquire_for(timeout);
      ASSERT_TRUE(maybe_borrowed.has_value());
      Base::CheckLocked(true);  // Ensure the lock is held.
      auto& borrowed = *maybe_borrowed;
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
    }
    Base::CheckLocked(false);  // Ensure the lock is released.
  }

  void TestTryAcquireForFailure(chrono::SystemClock::duration timeout) {
    Base::Lock();
    std::optional<BorrowedPointer<Derived, LockType>> maybe_borrowed =
        Base::borrowable_.try_acquire_for(timeout);
    EXPECT_FALSE(maybe_borrowed.has_value());
    Base::Unlock();
  }

  void TestTryAcquireUntilSuccess(chrono::SystemClock::duration timeout) {
    {
      auto deadline = chrono::SystemClock::now() + timeout;
      std::optional<BorrowedPointer<Derived, LockType>> maybe_borrowed =
          Base::borrowable_.try_acquire_until(deadline);
      ASSERT_TRUE(maybe_borrowed.has_value());
      Base::CheckLocked(true);  // Ensure the lock is held.
      auto& borrowed = *maybe_borrowed;
      EXPECT_EQ(borrowed->value(), Derived::kInitialValue);
    }
    Base::CheckLocked(false);  // Ensure the lock is released.
  }

  void TestTryAcquireUntilFailure(chrono::SystemClock::duration timeout) {
    Base::Lock();
    auto deadline = chrono::SystemClock::now() + timeout;
    std::optional<BorrowedPointer<Derived, LockType>> maybe_borrowed =
        Base::borrowable_.try_acquire_until(deadline);
    EXPECT_FALSE(maybe_borrowed.has_value());
    Base::Unlock();
  }
};

}  // namespace pw::sync::test
