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
#include "pw_sync/internal/timed_threaded_testing.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_unit_test/framework.h"

using pw::chrono::SystemClock;
using namespace std::chrono_literals;

namespace pw::sync {
namespace {

class TimedThreadNotificationTest : public test::TimedOptionallyThreadedTest {
 protected:
  TimedThreadNotification notification_;

  void Use(TimedThreadNotification& notification) { releaser_ = &notification; }

 private:
  void DoStop() override {
    // Multiple releases are the same as one.
    releaser_->release();
    releaser_->release();
  }

  TimedThreadNotification* releaser_ = &notification_;
};

// We can't control the SystemClock's period configuration, so just in case
// duration cannot be accurately expressed in integer ticks, round the
// duration up.
//
// Note that we can only test that calls take *at least* as long as expected.
// Differences between platforms and their schedulers prevent us from making
// guarantees that calls will complete *within* specific timeframes.
constexpr SystemClock::duration kShortDuration = SystemClock::for_at_least(1ms);
constexpr SystemClock::duration kLongDuration = SystemClock::for_at_least(42ms);

TEST_F(TimedThreadNotificationTest, EmptyInitialState) {
  EXPECT_FALSE(notification_.try_acquire());
}

TEST_F(TimedThreadNotificationTest, Release) {
  RunOnce();
  notification_.acquire();
  // Ensure it fails when not notified.
  EXPECT_FALSE(notification_.try_acquire());
}

TimedThreadNotification empty_initial_notification;
TEST_F(TimedThreadNotificationTest, EmptyInitialStateStatic) {
  EXPECT_FALSE(empty_initial_notification.try_acquire());
}

TimedThreadNotification raise_notification;
TEST_F(TimedThreadNotificationTest, ReleaseStatic) {
  Use(raise_notification);
  RunOnce();
  raise_notification.acquire();
  // Ensure it fails when not notified.
  EXPECT_FALSE(raise_notification.try_acquire());
}

TEST_F(TimedThreadNotificationTest, TryAcquireForNotified) {
  // Ensure it doesn't block and succeeds when notified.
  RunOnce();
  EXPECT_TRUE(notification_.try_acquire_for(kLongDuration));
}

TEST_F(TimedThreadNotificationTest, TryAcquireForNotifiedDelayed) {
  RunOnceAfter(kShortDuration);
  auto time_elapsed = Measure(
      [&]() { EXPECT_TRUE(notification_.try_acquire_for(kLongDuration)); });
  EXPECT_GE(time_elapsed, 0ms);
}

TEST_F(TimedThreadNotificationTest, TryAcquireForNotNotifiedPositiveTimeout) {
  // Ensure it blocks and fails when not notified for the full timeout.
  auto time_elapsed = Measure(
      [&]() { EXPECT_FALSE(notification_.try_acquire_for(kLongDuration)); });
  EXPECT_GE(time_elapsed, kLongDuration);
}

TEST_F(TimedThreadNotificationTest, TryAcquireForNotNotifiedZeroLengthTimeout) {
  // Ensure it doesn't block when a zero length duration is used.
  EXPECT_FALSE(notification_.try_acquire_for(0ms));
}

TEST_F(TimedThreadNotificationTest, TryAcquireForNotNotifiedNegativeTimeout) {
  // Ensure it doesn't block when a negative duration is used.
  EXPECT_FALSE(notification_.try_acquire_for(-kLongDuration));
}

TEST_F(TimedThreadNotificationTest, TryAcquireUntilNotified) {
  // Ensure it doesn't block and succeeds when notified.
  RunOnce();
  EXPECT_TRUE(notification_.try_acquire_until(
      SystemClock::TimePointAfterAtLeast(kLongDuration)));
}

TEST_F(TimedThreadNotificationTest, TryAcquireUntilNotifiedDelayed) {
  // Ensure it doesn't block and succeeds when notified.
  RunOnceAt(SystemClock::now() + kShortDuration);
  auto time_elapsed = Measure([&]() {
    EXPECT_TRUE(notification_.try_acquire_until(
        SystemClock::TimePointAfterAtLeast(kLongDuration)));
  });
  EXPECT_GE(time_elapsed, 0ms);
}

TEST_F(TimedThreadNotificationTest, TryAcquireUntilNotNotifiedFutureDeadline) {
  // Ensure it blocks and fails when not notified.
  auto time_elapsed = Measure([&]() {
    EXPECT_FALSE(notification_.try_acquire_until(
        SystemClock::TimePointAfterAtLeast(kLongDuration)));
  });
  EXPECT_GE(time_elapsed, kLongDuration);
}

TEST_F(TimedThreadNotificationTest, TryAcquireUntilNotNotifiedCurrentDeadline) {
  // Ensure it doesn't block when now is used.
  EXPECT_FALSE(notification_.try_acquire_until(SystemClock::now()));
}

TEST_F(TimedThreadNotificationTest, TryAcquireUntilNotNotifiedPastDeadline) {
  // Ensure it doesn't block when a timestamp in the past is used.
  EXPECT_FALSE(
      notification_.try_acquire_until(SystemClock::now() - kLongDuration));
}

}  // namespace
}  // namespace pw::sync
