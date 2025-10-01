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

#include "pw_sync/scoped_locker.h"

#include <mutex>

#include "pw_sync/test/lock_testing.h"
#include "pw_unit_test/framework.h"

using pw::ScopedLocker;
using pw::sync::test::FakeBasicLockable;

namespace {

TEST(ScopedLockerTest, AcquireConstructionAndLockedDestruction) {
  FakeBasicLockable lock;
  ASSERT_FALSE(lock.locked());
  {
    ScopedLocker locker(lock);
    EXPECT_TRUE(lock.locked());
  }
  EXPECT_FALSE(lock.locked());
}

TEST(ScopedLockerTest, DeferConstructionAndUnlockedDestruction) {
  FakeBasicLockable lock;
  ASSERT_FALSE(lock.locked());
  {
    ScopedLocker locker(lock, std::defer_lock);
    EXPECT_FALSE(lock.locked());
  }
  EXPECT_FALSE(lock.locked());
}

TEST(ScopedLockerTest, LockAndDestruction) {
  FakeBasicLockable lock;
  ASSERT_FALSE(lock.locked());
  {
    ScopedLocker locker(lock, std::defer_lock);
    EXPECT_FALSE(lock.locked());
    locker.lock();
    EXPECT_TRUE(lock.locked());
  }
  EXPECT_FALSE(lock.locked());
}

TEST(ScopedLockerTest, UnlockAndDestruction) {
  FakeBasicLockable lock;
  ASSERT_FALSE(lock.locked());
  {
    ScopedLocker locker(lock);
    EXPECT_TRUE(lock.locked());
    locker.unlock();
    EXPECT_FALSE(lock.locked());
  }
  EXPECT_FALSE(lock.locked());
}

}  // namespace
