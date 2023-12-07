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

#include "pw_sync/lock_traits.h"

#include "pw_sync/lock_testing.h"
#include "pw_unit_test/framework.h"

namespace pw::sync::test {

struct NotALock {};

TEST(LockTraitsTest, IsBasicLockable) {
  EXPECT_FALSE(is_basic_lockable_v<NotALock>);
  EXPECT_TRUE(is_basic_lockable_v<FakeBasicLockable>);
  EXPECT_TRUE(is_basic_lockable_v<FakeLockable>);
  EXPECT_TRUE(is_basic_lockable_v<FakeTimedLockable>);
}

TEST(LockTraitsTest, IsLockable) {
  EXPECT_FALSE(is_lockable_v<NotALock>);
  EXPECT_FALSE(is_lockable_v<FakeBasicLockable>);
  EXPECT_TRUE(is_lockable_v<FakeLockable>);
  EXPECT_TRUE(is_lockable_v<FakeTimedLockable>);
}

TEST(LockTraitsTest, IsLockableFor) {
  EXPECT_FALSE((is_lockable_for_v<NotALock, FakeClock::duration>));
  EXPECT_FALSE((is_lockable_for_v<FakeBasicLockable, FakeClock::duration>));
  EXPECT_FALSE((is_lockable_for_v<FakeLockable, FakeClock::duration>));
  EXPECT_FALSE((is_lockable_for_v<FakeTimedLockable, NotAClock::duration>));
  EXPECT_TRUE((is_lockable_for_v<FakeTimedLockable, FakeClock::duration>));
}

TEST(LockTraitsTest, IsLockableUntil) {
  EXPECT_FALSE((is_lockable_until_v<NotALock, FakeClock::time_point>));
  EXPECT_FALSE((is_lockable_until_v<FakeBasicLockable, FakeClock::time_point>));
  EXPECT_FALSE((is_lockable_until_v<FakeLockable, FakeClock::time_point>));
  EXPECT_FALSE((is_lockable_until_v<FakeTimedLockable, NotAClock::time_point>));
  EXPECT_TRUE((is_lockable_until_v<FakeTimedLockable, FakeClock::time_point>));
}

TEST(LockTraitsTest, IsTimedLockable) {
  EXPECT_FALSE((is_timed_lockable_v<NotALock, FakeClock>));
  EXPECT_FALSE((is_timed_lockable_v<FakeBasicLockable, FakeClock>));
  EXPECT_FALSE((is_timed_lockable_v<FakeLockable, FakeClock>));
  EXPECT_FALSE((is_timed_lockable_v<FakeTimedLockable, NotAClock>));
  EXPECT_TRUE((is_timed_lockable_v<FakeTimedLockable, FakeClock>));
}

}  // namespace pw::sync::test
