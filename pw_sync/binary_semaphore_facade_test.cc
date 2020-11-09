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
#include "pw_sync/binary_semaphore.h"

using pw::chrono::SystemClock;

namespace pw::sync {
namespace {

extern "C" {

// Functions defined in binary_semaphore_facade_test_c.c which call the API
// from C.
void pw_sync_BinarySemaphore_CallRelease(pw_sync_BinarySemaphore* semaphore);
void pw_sync_BinarySemaphore_CallAcquire(pw_sync_BinarySemaphore* semaphore);
bool pw_sync_BinarySemaphore_CallTryAcquire(pw_sync_BinarySemaphore* semaphore);
bool pw_sync_BinarySemaphore_CallTryAcquireFor(
    pw_sync_BinarySemaphore* semaphore,
    pw_chrono_SystemClock_TickCount for_at_least);
bool pw_sync_BinarySemaphore_CallTryAcquireUntil(
    pw_sync_BinarySemaphore* semaphore,
    pw_chrono_SystemClock_TimePoint until_at_least);
ptrdiff_t pw_sync_BinarySemaphore_CallMax(void);

}  // extern "C"

static constexpr auto kArbitraryDuration = std::chrono::milliseconds(42);
// We can't control the SystemClock's period configuration, so just in case
// duration cannot be accurately expressed in integer ticks, round the
// duration w/ duration_cast.
static constexpr auto kRoundedArbitraryDuration =
    std::chrono::duration_cast<SystemClock::duration>(kArbitraryDuration);
static constexpr pw_chrono_SystemClock_TickCount kRoundedArbitraryDurationInC =
    kRoundedArbitraryDuration.count();

TEST(BinarySemaphore, EmptyInitialState) {
  BinarySemaphore semaphore;
  EXPECT_FALSE(semaphore.try_acquire());
}

// TODO(pwbug/291): Add real concurrency tests once we have pw::thread.

TEST(BinarySemaphore, Release) {
  BinarySemaphore semaphore;
  semaphore.release();
  semaphore.release();
  semaphore.acquire();
  // Ensure it fails when empty.
  EXPECT_FALSE(semaphore.try_acquire());
}

BinarySemaphore empty_initial_semaphore;
TEST(BinarySemaphore, EmptyInitialStateStatic) {
  EXPECT_FALSE(empty_initial_semaphore.try_acquire());
}

BinarySemaphore release_semaphore;
TEST(BinarySemaphore, ReleaseStatic) {
  release_semaphore.release();
  release_semaphore.release();
  release_semaphore.acquire();
  // Ensure it fails when empty.
  EXPECT_FALSE(release_semaphore.try_acquire());
}

TEST(BinarySemaphore, TryAcquireFor) {
  BinarySemaphore semaphore;
  semaphore.release();

  SystemClock::time_point before = SystemClock::now();
  EXPECT_TRUE(semaphore.try_acquire_for(kRoundedArbitraryDuration));
  SystemClock::duration time_elapsed = SystemClock::now() - before;
  EXPECT_LT(time_elapsed, kRoundedArbitraryDuration);

  // Ensure it blocks and fails when empty.
  before = SystemClock::now();
  EXPECT_FALSE(semaphore.try_acquire_for(kRoundedArbitraryDuration));
  time_elapsed = SystemClock::now() - before;
  EXPECT_GE(time_elapsed, kRoundedArbitraryDuration);
}

TEST(BinarySemaphore, TryAcquireUntil) {
  BinarySemaphore semaphore;
  semaphore.release();

  const SystemClock::time_point deadline =
      SystemClock::now() + kRoundedArbitraryDuration;
  EXPECT_TRUE(semaphore.try_acquire_until(deadline));
  EXPECT_LT(SystemClock::now(), deadline);

  // Ensure it blocks and fails when empty.
  EXPECT_FALSE(semaphore.try_acquire_until(deadline));
  EXPECT_GE(SystemClock::now(), deadline);
}

TEST(BinarySemaphore, EmptyInitialStateInC) {
  BinarySemaphore semaphore;
  EXPECT_FALSE(pw_sync_BinarySemaphore_CallTryAcquire(&semaphore));
}

TEST(BinarySemaphore, ReleaseInC) {
  BinarySemaphore semaphore;
  pw_sync_BinarySemaphore_CallRelease(&semaphore);
  pw_sync_BinarySemaphore_CallRelease(&semaphore);
  pw_sync_BinarySemaphore_CallAcquire(&semaphore);
  // Ensure it fails when empty.
  EXPECT_FALSE(pw_sync_BinarySemaphore_CallTryAcquire(&semaphore));
}

TEST(BinarySemaphore, TryAcquireForInC) {
  BinarySemaphore semaphore;
  pw_sync_BinarySemaphore_CallRelease(&semaphore);

  pw_chrono_SystemClock_TimePoint before = pw_chrono_SystemClock_Now();
  ASSERT_TRUE(pw_sync_BinarySemaphore_CallTryAcquireFor(
      &semaphore, kRoundedArbitraryDurationInC));
  pw_chrono_SystemClock_TickCount time_elapsed =
      pw_chrono_SystemClock_Now().ticks_since_epoch - before.ticks_since_epoch;
  EXPECT_LT(time_elapsed, kRoundedArbitraryDurationInC);

  // Ensure it blocks and fails when empty.
  before = pw_chrono_SystemClock_Now();
  EXPECT_FALSE(pw_sync_BinarySemaphore_CallTryAcquireFor(
      &semaphore, kRoundedArbitraryDurationInC));
  time_elapsed =
      pw_chrono_SystemClock_Now().ticks_since_epoch - before.ticks_since_epoch;
  EXPECT_GE(time_elapsed, kRoundedArbitraryDurationInC);
}

TEST(BinarySemaphore, TryAcquireUntilInC) {
  BinarySemaphore semaphore;
  pw_sync_BinarySemaphore_CallRelease(&semaphore);

  pw_chrono_SystemClock_TimePoint deadline;
  deadline.ticks_since_epoch = pw_chrono_SystemClock_Now().ticks_since_epoch +
                               kRoundedArbitraryDurationInC;
  ASSERT_TRUE(
      pw_sync_BinarySemaphore_CallTryAcquireUntil(&semaphore, deadline));
  EXPECT_LT(pw_chrono_SystemClock_Now().ticks_since_epoch,
            deadline.ticks_since_epoch);

  // Ensure it blocks and fails when empty.
  EXPECT_FALSE(
      pw_sync_BinarySemaphore_CallTryAcquireUntil(&semaphore, deadline));
  EXPECT_GE(pw_chrono_SystemClock_Now().ticks_since_epoch,
            deadline.ticks_since_epoch);
}

TEST(BinarySemaphore, MaxInC) {
  EXPECT_EQ(BinarySemaphore::max(), pw_sync_BinarySemaphore_Max());
}

}  // namespace
}  // namespace pw::sync
