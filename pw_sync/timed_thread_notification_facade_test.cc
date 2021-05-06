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
#include "pw_sync/timed_thread_notification.h"

using pw::chrono::SystemClock;
using namespace std::chrono_literals;

namespace pw::sync {
namespace {

// We can't control the SystemClock's period configuration, so just in case
// duration cannot be accurately expressed in integer ticks, round the
// duration up.
constexpr SystemClock::duration kRoundedArbitraryDuration =
    SystemClock::for_at_least(42ms);

TEST(TimedThreadNotification, EmptyInitialState) {
  TimedThreadNotification notification;
  EXPECT_FALSE(notification.try_acquire());
}

// TODO(pwbug/291): Add real concurrency tests.

TEST(TimedThreadNotification, Release) {
  TimedThreadNotification notification;
  notification.release();
  notification.release();
  notification.acquire();
  // Ensure it fails when not notified.
  EXPECT_FALSE(notification.try_acquire());
}

TimedThreadNotification empty_initial_notification;
TEST(TimedThreadNotification, EmptyInitialStateStatic) {
  EXPECT_FALSE(empty_initial_notification.try_acquire());
}

TimedThreadNotification raise_notification;
TEST(TimedThreadNotification, ReleaseStatic) {
  raise_notification.release();
  raise_notification.release();
  raise_notification.acquire();
  // Ensure it fails when not notified.
  EXPECT_FALSE(raise_notification.try_acquire());
}

TEST(TimedThreadNotification, TryAcquireFor) {
  TimedThreadNotification notification;
  notification.release();

  SystemClock::time_point before = SystemClock::now();
  EXPECT_TRUE(notification.try_acquire_for(kRoundedArbitraryDuration));
  SystemClock::duration time_elapsed = SystemClock::now() - before;
  EXPECT_LT(time_elapsed, kRoundedArbitraryDuration);

  // Ensure it blocks and fails when not notified.
  before = SystemClock::now();
  EXPECT_FALSE(notification.try_acquire_for(kRoundedArbitraryDuration));
  time_elapsed = SystemClock::now() - before;
  EXPECT_GE(time_elapsed, kRoundedArbitraryDuration);
}

TEST(TimedThreadNotification, tryAcquireUntil) {
  TimedThreadNotification notification;
  notification.release();

  const SystemClock::time_point deadline =
      SystemClock::now() + kRoundedArbitraryDuration;
  EXPECT_TRUE(notification.try_acquire_until(deadline));
  EXPECT_LT(SystemClock::now(), deadline);

  // Ensure it blocks and fails when not notified.
  EXPECT_FALSE(notification.try_acquire_until(deadline));
  EXPECT_GE(SystemClock::now(), deadline);
}

}  // namespace
}  // namespace pw::sync
