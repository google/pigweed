// Copyright 2020 The Pigweed Authors
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

#include <chrono>

#include "gtest/gtest.h"
#include "pw_chrono/system_clock.h"
#include "pw_sync/mutex.h"

using pw::chrono::SystemClock;
using namespace std::chrono_literals;

namespace pw::sync {
namespace {

extern "C" {

// Functions defined in mutex_facade_test_c.c which call the API from C.
void pw_sync_Mutex_CallLock(pw_sync_Mutex* mutex);
bool pw_sync_Mutex_CallTryLock(pw_sync_Mutex* mutex);
bool pw_sync_Mutex_CallTryLockFor(pw_sync_Mutex* mutex,
                                  pw_chrono_SystemClock_Duration for_at_least);
bool pw_sync_Mutex_CallTryLockUntil(
    pw_sync_Mutex* mutex, pw_chrono_SystemClock_TimePoint until_at_least);
void pw_sync_Mutex_CallUnlock(pw_sync_Mutex* mutex);

}  // extern "C"

// We can't control the SystemClock's period configuration, so just in case
// duration cannot be accurately expressed in integer ticks, round the
// duration up.
constexpr SystemClock::duration kRoundedArbitraryDuration =
    SystemClock::for_at_least(42ms);
constexpr pw_chrono_SystemClock_Duration kRoundedArbitraryDurationInC =
    PW_SYSTEM_CLOCK_MS(42);

// TODO(pwbug/291): Add real concurrency tests once we have pw::thread.

TEST(Mutex, LockUnlock) {
  pw::sync::Mutex mutex;
  mutex.lock();
  // TODO(pwbug/291): Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

Mutex static_mutex;
TEST(Mutex, LockUnlockStatic) {
  static_mutex.lock();
  // TODO(pwbug/291): Ensure it fails to lock when already held.
  // EXPECT_FALSE(static_mutex.try_lock());
  static_mutex.unlock();
}

TEST(Mutex, TryLockUnlock) {
  pw::sync::Mutex mutex;
  ASSERT_TRUE(mutex.try_lock());
  // TODO(pwbug/291): Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

TEST(Mutex, TryLockUnlockFor) {
  pw::sync::Mutex mutex;

  SystemClock::time_point before = SystemClock::now();
  ASSERT_TRUE(mutex.try_lock_for(kRoundedArbitraryDuration));
  SystemClock::duration time_elapsed = SystemClock::now() - before;
  EXPECT_LT(time_elapsed, kRoundedArbitraryDuration);

  // TODO(pwbug/291): Ensure it blocks fails to lock when already held.
  // before = SystemClock::now();
  // EXPECT_FALSE(mutex.try_lock_for(kRoundedArbitraryDuration));
  // time_elapsed = SystemClock::now() - before;
  /// EXPECT_GE(time_elapsed, kRoundedArbitraryDuration);

  mutex.unlock();
}

TEST(Mutex, TryLockUnlockUntil) {
  pw::sync::Mutex mutex;

  const SystemClock::time_point deadline =
      SystemClock::now() + kRoundedArbitraryDuration;
  ASSERT_TRUE(mutex.try_lock_until(deadline));
  EXPECT_LT(SystemClock::now(), deadline);

  // TODO(pwbug/291): Ensure it blocks fails to lock when already held.
  // EXPECT_FALSE(
  //     mutex.try_lock_until(SystemClock::now() + kRoundedArbitraryDuration));
  // EXPECT_GE(SystemClock::now(), deadline);

  mutex.unlock();
}

TEST(Mutex, LockUnlockInC) {
  pw::sync::Mutex mutex;
  pw_sync_Mutex_CallLock(&mutex);
  pw_sync_Mutex_CallUnlock(&mutex);
}

TEST(Mutex, TryLockUnlockInC) {
  pw::sync::Mutex mutex;
  ASSERT_TRUE(pw_sync_Mutex_CallTryLock(&mutex));
  // TODO(pwbug/291): Ensure it fails to lock when already held.
  // EXPECT_FALSE(pw_sync_Mutex_CallTryLock(&mutex));
  pw_sync_Mutex_CallUnlock(&mutex);
}

TEST(Mutex, TryLockUnlockForInC) {
  pw::sync::Mutex mutex;

  pw_chrono_SystemClock_TimePoint before = pw_chrono_SystemClock_Now();
  ASSERT_TRUE(
      pw_sync_Mutex_CallTryLockFor(&mutex, kRoundedArbitraryDurationInC));
  pw_chrono_SystemClock_Duration time_elapsed =
      pw_chrono_SystemClock_TimeElapsed(before, pw_chrono_SystemClock_Now());
  EXPECT_LT(time_elapsed.ticks, kRoundedArbitraryDurationInC.ticks);

  // TODO(pwbug/291): Ensure it blocks fails to lock when already held.
  // before = pw_chrono_SystemClock_Now();
  // EXPECT_FALSE(
  //     pw_sync_Mutex_CallTryLockFor(&mutex, kRoundedArbitraryDurationInC));
  // time_elapsed =
  //    pw_chrono_SystemClock_TimeElapsed(before, pw_chrono_SystemClock_Now());
  // EXPECT_GE(time_elapsed.ticks, kRoundedArbitraryDurationInC.ticks);

  pw_sync_Mutex_CallUnlock(&mutex);
}

TEST(Mutex, TryLockUnlockUntilInC) {
  pw::sync::Mutex mutex;
  pw_chrono_SystemClock_TimePoint deadline;
  deadline.duration_since_epoch.ticks =
      pw_chrono_SystemClock_Now().duration_since_epoch.ticks +
      kRoundedArbitraryDurationInC.ticks;
  ASSERT_TRUE(pw_sync_Mutex_CallTryLockUntil(&mutex, deadline));
  EXPECT_LT(pw_chrono_SystemClock_Now().duration_since_epoch.ticks,
            deadline.duration_since_epoch.ticks);

  // TODO(pwbug/291): Ensure it blocks fails to lock when already held.
  // EXPECT_FALSE(pw_sync_Mutex_CallTryLockUntil(&mutex, deadline));
  // EXPECT_GE(pw_chrono_SystemClock_Now().duration_since_epoch.ticks,
  //           deadline.duration_since_epoch.ticks);

  mutex.unlock();
}

}  // namespace
}  // namespace pw::sync
