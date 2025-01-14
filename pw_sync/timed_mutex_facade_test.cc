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

#include "pw_chrono/system_clock.h"
#include "pw_sync/timed_borrow_testing.h"
#include "pw_sync/timed_mutex.h"
#include "pw_unit_test/framework.h"

using pw::chrono::SystemClock;
using namespace std::chrono_literals;

namespace pw::sync {
namespace {

extern "C" {

// Functions defined in mutex_facade_test_c.c which call the API from C.
void pw_sync_TimedMutex_CallLock(pw_sync_TimedMutex* mutex);
bool pw_sync_TimedMutex_CallTryLock(pw_sync_TimedMutex* mutex);
bool pw_sync_TimedMutex_CallTryLockFor(pw_sync_TimedMutex* mutex,
                                       pw_chrono_SystemClock_Duration timeout);
bool pw_sync_TimedMutex_CallTryLockUntil(
    pw_sync_TimedMutex* mutex, pw_chrono_SystemClock_TimePoint deadline);
void pw_sync_TimedMutex_CallUnlock(pw_sync_TimedMutex* mutex);

}  // extern "C"

// We can't control the SystemClock's period configuration, so just in case
// duration cannot be accurately expressed in integer ticks, round the
// duration up.
constexpr SystemClock::duration kRoundedArbitraryDuration =
    SystemClock::for_at_least(42ms);
constexpr pw_chrono_SystemClock_Duration kRoundedArbitraryDurationInC =
    PW_SYSTEM_CLOCK_MS(42);

// TODO: b/235284163 - Add real concurrency tests once we have pw::thread.

TEST(TimedMutex, LockUnlock) {
  TimedMutex mutex;
  mutex.lock();
  mutex.unlock();
  // TODO: b/235284163 - Ensure it fails to lock when already held by someone
  // else.
  // EXPECT_FALSE(mutex.try_lock());
}

TimedMutex static_mutex;
TEST(TimedMutex, LockUnlockStatic) {
  static_mutex.lock();
  static_mutex.unlock();
  // TODO: b/235284163 - Ensure it fails to lock when already held by someone
  // else.
  // EXPECT_FALSE(static_mutex.try_lock());
}

TEST(TimedMutex, TryLockUnlock) {
  TimedMutex mutex;
  const bool locked = mutex.try_lock();
  EXPECT_TRUE(locked);
  if (locked) {
    // EXPECT_FALSE(mutex.try_lock());
    mutex.unlock();
  }
  // TODO: b/235284163 - Ensure it fails to lock when already held by someone
  // else.
}

TEST(TimedMutex, TryLockUnlockFor) {
  TimedMutex mutex;

  SystemClock::time_point before = SystemClock::now();
  const bool locked = mutex.try_lock_for(kRoundedArbitraryDuration);
  EXPECT_TRUE(locked);
  if (locked) {
    SystemClock::duration time_elapsed = SystemClock::now() - before;
    EXPECT_LT(time_elapsed, kRoundedArbitraryDuration);
    mutex.unlock();
  }
  // TODO: b/235284163 - Ensure it blocks and fails to lock when already held by
  // someone else.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and a zero length duration is used.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and a negative duration is used.
}

TEST(TimedMutex, TryLockUnlockUntil) {
  TimedMutex mutex;

  const SystemClock::time_point deadline =
      SystemClock::now() + kRoundedArbitraryDuration;
  const bool locked = mutex.try_lock_until(deadline);
  EXPECT_TRUE(locked);
  if (locked) {
    EXPECT_LT(SystemClock::now(), deadline);
    mutex.unlock();
  }
  // TODO: b/235284163 - Ensure it blocks and fails to lock when already held by
  // someone else.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and now is used.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and a timestamp in the past is used.
}

// Unit tests for a `Borrowable`that uses a `TimedMutex` as its lock.
using TimedMutexBorrowTest = test::TimedBorrowTest<TimedMutex>;

TEST_F(TimedMutexBorrowTest, Acquire) { TestAcquire(); }

TEST_F(TimedMutexBorrowTest, ConstAcquire) { TestConstAcquire(); }

TEST_F(TimedMutexBorrowTest, RepeatedAcquire) { TestRepeatedAcquire(); }

TEST_F(TimedMutexBorrowTest, Moveable) { TestMoveable(); }

TEST_F(TimedMutexBorrowTest, Copyable) { TestCopyable(); }

TEST_F(TimedMutexBorrowTest, CopyableCovariant) { TestCopyableCovariant(); }

TEST_F(TimedMutexBorrowTest, TryAcquireSuccess) { TestTryAcquireSuccess(); }

TEST_F(TimedMutexBorrowTest, TryAcquireFailure) { TestTryAcquireFailure(); }

TEST_F(TimedMutexBorrowTest, TryAcquireForSuccess) {
  TestTryAcquireForSuccess(kRoundedArbitraryDuration);
}

TEST_F(TimedMutexBorrowTest, TryAcquireForFailure) {
  TestTryAcquireForFailure(kRoundedArbitraryDuration);
}

TEST_F(TimedMutexBorrowTest, TryAcquireUntilSuccess) {
  TestTryAcquireUntilSuccess(kRoundedArbitraryDuration);
}

TEST_F(TimedMutexBorrowTest, TryAcquireUntilFailure) {
  TestTryAcquireUntilFailure(kRoundedArbitraryDuration);
}

TEST(VirtualTimedMutex, LockUnlock) {
  VirtualTimedMutex mutex;
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held by someone
  // else.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

VirtualTimedMutex static_virtual_mutex;
TEST(VirtualTimedMutex, LockUnlockStatic) {
  static_virtual_mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held by someone
  // else.
  // EXPECT_FALSE(static_virtual_mutex.try_lock());
  static_virtual_mutex.unlock();
}

TEST(VirtualMutex, LockUnlockExternal) {
  VirtualTimedMutex virtual_timed_mutex;
  auto& mutex = virtual_timed_mutex.timed_mutex();
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

// Unit tests for a `Borrowable`that uses a `VirtualTimedMutex` as its lock.
using VirtualTimedMutexBorrowTest = test::TimedBorrowTest<VirtualTimedMutex>;

TEST_F(VirtualTimedMutexBorrowTest, Acquire) { TestAcquire(); }

TEST_F(VirtualTimedMutexBorrowTest, ConstAcquire) { TestConstAcquire(); }

TEST_F(VirtualTimedMutexBorrowTest, RepeatedAcquire) { TestRepeatedAcquire(); }

TEST_F(VirtualTimedMutexBorrowTest, Moveable) { TestMoveable(); }

TEST_F(VirtualTimedMutexBorrowTest, Copyable) { TestCopyable(); }

TEST_F(VirtualTimedMutexBorrowTest, CopyableCovariant) {
  TestCopyableCovariant();
}

TEST_F(VirtualTimedMutexBorrowTest, TryAcquireSuccess) {
  TestTryAcquireSuccess();
}

TEST_F(VirtualTimedMutexBorrowTest, TryAcquireFailure) {
  TestTryAcquireFailure();
}

TEST_F(VirtualTimedMutexBorrowTest, TryAcquireForSuccess) {
  TestTryAcquireForSuccess(kRoundedArbitraryDuration);
}

TEST_F(VirtualTimedMutexBorrowTest, TryAcquireForFailure) {
  TestTryAcquireForFailure(kRoundedArbitraryDuration);
}

TEST_F(VirtualTimedMutexBorrowTest, TryAcquireUntilSuccess) {
  TestTryAcquireUntilSuccess(kRoundedArbitraryDuration);
}

TEST_F(VirtualTimedMutexBorrowTest, TryAcquireUntilFailure) {
  TestTryAcquireUntilFailure(kRoundedArbitraryDuration);
}

TEST(TimedMutex, LockUnlockInC) {
  TimedMutex mutex;
  pw_sync_TimedMutex_CallLock(&mutex);
  pw_sync_TimedMutex_CallUnlock(&mutex);
}

TEST(TimedMutex, TryLockUnlockInC) {
  TimedMutex mutex;
  ASSERT_TRUE(pw_sync_TimedMutex_CallTryLock(&mutex));
  // TODO: b/235284163 - Ensure it fails to lock when already held by someone
  // else.
  // EXPECT_FALSE(pw_sync_TimedMutex_CallTryLock(&mutex));
  pw_sync_TimedMutex_CallUnlock(&mutex);
}

TEST(TimedMutex, TryLockUnlockForInC) {
  TimedMutex mutex;

  pw_chrono_SystemClock_TimePoint before = pw_chrono_SystemClock_Now();
  ASSERT_TRUE(
      pw_sync_TimedMutex_CallTryLockFor(&mutex, kRoundedArbitraryDurationInC));
  pw_chrono_SystemClock_Duration time_elapsed =
      pw_chrono_SystemClock_TimeElapsed(before, pw_chrono_SystemClock_Now());
  EXPECT_LT(time_elapsed.ticks, kRoundedArbitraryDurationInC.ticks);
  pw_sync_TimedMutex_CallUnlock(&mutex);
  // TODO: b/235284163 - Ensure it blocks and fails to lock when already held by
  // someone else.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and a zero length duration is used.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and a negative duration is used.
}

TEST(TimedMutex, TryLockUnlockUntilInC) {
  TimedMutex mutex;
  pw_chrono_SystemClock_TimePoint deadline;
  deadline.duration_since_epoch.ticks =
      pw_chrono_SystemClock_Now().duration_since_epoch.ticks +
      kRoundedArbitraryDurationInC.ticks;
  ASSERT_TRUE(pw_sync_TimedMutex_CallTryLockUntil(&mutex, deadline));
  EXPECT_LT(pw_chrono_SystemClock_Now().duration_since_epoch.ticks,
            deadline.duration_since_epoch.ticks);
  pw_sync_TimedMutex_CallUnlock(&mutex);
  // TODO: b/235284163 - Ensure it blocks and fails to lock when already held by
  // someone else.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and now is used.
  // TODO: b/235284163 - Ensure it does not block and fails to lock when already
  // held by someone else and a timestamp in the past is used.
}

}  // namespace
}  // namespace pw::sync
