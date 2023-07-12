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

#include "pw_sync/borrow.h"

#include <chrono>
#include <ratio>

#include "gtest/gtest.h"
#include "pw_sync/lock_testing.h"

namespace pw::sync::test {
namespace {

template <typename Lock>
class BorrowableTest : public ::testing::Test {
 protected:
  static constexpr int kInitialBaseValue = 24;
  static constexpr int kInitialValue = 42;

  BorrowableTest()
      : foo_{{kInitialBaseValue}, kInitialValue},
        borrowable_foo_(foo_, lock_) {}

  void SetUp() override {
    EXPECT_FALSE(lock_.locked());  // Ensure it's not locked on construction.
  }
  struct Base {
    int base_value;
  };
  struct Foo : public Base {
    int value;
  };
  Lock lock_;
  Foo foo_;
  Borrowable<Foo, Lock> borrowable_foo_;
};

using BorrowableBasicLockableTest = BorrowableTest<FakeBasicLockable>;

TEST_F(BorrowableBasicLockableTest, Acquire) {
  {
    BorrowedPointer<Foo, FakeBasicLockable> borrowed_foo =
        borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  EXPECT_EQ(foo_.value, 13);
}

TEST_F(BorrowableBasicLockableTest, ConstAcquire) {
  const Borrowable<Foo, FakeBasicLockable> const_borrowable_foo(
      borrowable_foo_);
  {
    BorrowedPointer<Foo, FakeBasicLockable> borrowed_foo =
        const_borrowable_foo.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  EXPECT_EQ(foo_.value, 13);
}

TEST_F(BorrowableBasicLockableTest, RepeatedAcquire) {
  {
    BorrowedPointer<Foo, FakeBasicLockable> borrowed_foo =
        borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  {
    BorrowedPointer<Foo, FakeBasicLockable> borrowed_foo =
        borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, 13);
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableBasicLockableTest, Moveable) {
  Borrowable<Foo, FakeBasicLockable> borrowable_foo =
      std::move(borrowable_foo_);
  {
    BorrowedPointer<Foo, FakeBasicLockable> borrowed_foo =
        borrowable_foo.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableBasicLockableTest, Copyable) {
  const Borrowable<Foo, FakeBasicLockable>& other = borrowable_foo_;
  Borrowable<Foo, FakeBasicLockable> borrowable_foo(other);
  {
    BorrowedPointer<Foo, FakeBasicLockable> borrowed_foo =
        borrowable_foo.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  EXPECT_EQ(foo_.value, 13);
}

TEST_F(BorrowableBasicLockableTest, CopyableCovariant) {
  const Borrowable<Foo, FakeBasicLockable>& other = borrowable_foo_;
  Borrowable<Base, FakeBasicLockable> borrowable_base(other);
  {
    BorrowedPointer<Base, FakeBasicLockable> borrowed_base =
        borrowable_base.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_base->base_value, kInitialBaseValue);
    borrowed_base->base_value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  EXPECT_EQ(foo_.base_value, 13);
}

using BorrowableLockableTest = BorrowableTest<FakeLockable>;

TEST_F(BorrowableLockableTest, Acquire) {
  {
    BorrowedPointer<Foo, FakeLockable> borrowed_foo = borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  EXPECT_EQ(foo_.value, 13);
}

TEST_F(BorrowableLockableTest, RepeatedAcquire) {
  {
    BorrowedPointer<Foo, FakeLockable> borrowed_foo = borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  {
    BorrowedPointer<Foo, FakeLockable> borrowed_foo = borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, 13);
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableLockableTest, TryAcquireSuccess) {
  {
    std::optional<BorrowedPointer<Foo, FakeLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire();
    ASSERT_TRUE(maybe_borrowed_foo.has_value());
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(maybe_borrowed_foo.value()->value, kInitialValue);
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableLockableTest, TryAcquireFailure) {
  lock_.lock();
  EXPECT_TRUE(lock_.locked());
  {
    std::optional<BorrowedPointer<Foo, FakeLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire();
    EXPECT_FALSE(maybe_borrowed_foo.has_value());
  }
  EXPECT_TRUE(lock_.locked());
  lock_.unlock();
}

using BorrowableTimedLockableTest = BorrowableTest<FakeTimedLockable>;

TEST_F(BorrowableTimedLockableTest, Acquire) {
  {
    BorrowedPointer<Foo, FakeTimedLockable> borrowed_foo =
        borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  EXPECT_EQ(foo_.value, 13);
}

TEST_F(BorrowableTimedLockableTest, RepeatedAcquire) {
  {
    BorrowedPointer<Foo, FakeTimedLockable> borrowed_foo =
        borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, kInitialValue);
    borrowed_foo->value = 13;
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
  {
    BorrowedPointer<Foo, FakeTimedLockable> borrowed_foo =
        borrowable_foo_.acquire();
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(borrowed_foo->value, 13);
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableTimedLockableTest, TryAcquireSuccess) {
  {
    std::optional<BorrowedPointer<Foo, FakeTimedLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire();
    ASSERT_TRUE(maybe_borrowed_foo.has_value());
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(maybe_borrowed_foo.value()->value, kInitialValue);
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableTimedLockableTest, TryAcquireFailure) {
  lock_.lock();
  EXPECT_TRUE(lock_.locked());
  {
    std::optional<BorrowedPointer<Foo, FakeTimedLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire();
    EXPECT_FALSE(maybe_borrowed_foo.has_value());
  }
  EXPECT_TRUE(lock_.locked());
  lock_.unlock();
}

TEST_F(BorrowableTimedLockableTest, TryAcquireForSuccess) {
  {
    std::optional<BorrowedPointer<Foo, FakeTimedLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire_for(std::chrono::seconds(0));
    ASSERT_TRUE(maybe_borrowed_foo.has_value());
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(maybe_borrowed_foo.value()->value, kInitialValue);
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableTimedLockableTest, TryAcquireForFailure) {
  lock_.lock();
  EXPECT_TRUE(lock_.locked());
  {
    std::optional<BorrowedPointer<Foo, FakeTimedLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire_for(std::chrono::seconds(0));
    EXPECT_FALSE(maybe_borrowed_foo.has_value());
  }
  EXPECT_TRUE(lock_.locked());
  lock_.unlock();
}

TEST_F(BorrowableTimedLockableTest, TryAcquireUntilSuccess) {
  {
    std::optional<BorrowedPointer<Foo, FakeTimedLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire_until(FakeClock::time_point());
    ASSERT_TRUE(maybe_borrowed_foo.has_value());
    EXPECT_TRUE(lock_.locked());  // Ensure the lock is held.
    EXPECT_EQ(maybe_borrowed_foo.value()->value, kInitialValue);
  }
  EXPECT_FALSE(lock_.locked());  // Ensure the lock is released.
}

TEST_F(BorrowableTimedLockableTest, TryAcquireUntilFailure) {
  lock_.lock();
  EXPECT_TRUE(lock_.locked());
  {
    std::optional<BorrowedPointer<Foo, FakeTimedLockable>> maybe_borrowed_foo =
        borrowable_foo_.try_acquire_until(FakeClock::time_point());
    EXPECT_FALSE(maybe_borrowed_foo.has_value());
  }
  EXPECT_TRUE(lock_.locked());
  lock_.unlock();
}

}  // namespace
}  // namespace pw::sync::test
