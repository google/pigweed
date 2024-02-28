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

#include "pw_sync/lock_testing.h"
#include "pw_sync_private/borrow_lockable_tests.h"
#include "pw_unit_test/framework.h"

namespace pw::sync::test {
namespace {

// Test fixtures.

class Base {
 public:
  virtual ~Base() = default;
  int value() const { return value_; }

 protected:
  Base(int value) : value_(value) {}

 private:
  int value_ = 0;
};

class Derived : public Base {
 public:
  Derived(int value) : Base(value) {}
};

// Unit tests.

TEST(BorrowedPointerTest, MoveConstruct) {
  Derived derived(1);
  FakeBasicLockable lock;
  Borrowable<Derived, FakeBasicLockable> borrowable(derived, lock);
  BorrowedPointer<Base, VirtualBasicLockable> borrowed(borrowable.acquire());
  EXPECT_EQ(borrowed->value(), 1);
}

TEST(BorrowedPointerTest, MoveAssign) {
  Derived derived(2);
  FakeBasicLockable lock;
  Borrowable<Derived, FakeBasicLockable> borrowable(derived, lock);
  BorrowedPointer<Base, VirtualBasicLockable> borrowed = borrowable.acquire();
  EXPECT_EQ(borrowed->value(), 2);
}

PW_SYNC_ADD_BORROWABLE_LOCK_TESTS(FakeBasicLockable);
PW_SYNC_ADD_BORROWABLE_LOCK_TESTS(FakeLockable);
PW_SYNC_ADD_BORROWABLE_TIMED_LOCK_TESTS(FakeTimedLockable, FakeClock);

}  // namespace
}  // namespace pw::sync::test
