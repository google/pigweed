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
#include "pw_thread/id.h"
#include "pw_thread/sleep.h"

using pw::chrono::SystemClock;

namespace pw::this_thread {
namespace {

extern "C" {

// Functions defined in sleep_facade_test_c.c which call the API from C.
void pw_this_thread_CallSleepFor(pw_chrono_SystemClock_TickCount for_at_least);
void pw_this_thread_CallSleepUntil(
    pw_chrono_SystemClock_TimePoint until_at_least);

}  // extern "C"

static constexpr auto kArbitraryDuration = std::chrono::milliseconds(42);
// We can't control the SystemClock's period configuration, so just in case
// duration cannot be accurately expressed in integer ticks, round the
// duration w/ duration_cast.
static constexpr auto kRoundedArbitraryDuration =
    std::chrono::duration_cast<SystemClock::duration>(kArbitraryDuration);
static constexpr pw_chrono_SystemClock_TickCount kRoundedArbitraryDurationInC =
    kRoundedArbitraryDuration.count();

TEST(Sleep, SleepFor) {
  // Ensure we are in a thread context, meaning we are permitted to sleep.
  ASSERT_NE(get_id(), thread::Id());

  const SystemClock::time_point before = SystemClock::now();
  sleep_for(kRoundedArbitraryDuration);
  const SystemClock::duration time_elapsed = SystemClock::now() - before;
  EXPECT_GE(time_elapsed, kRoundedArbitraryDuration);
}

TEST(Sleep, SleepUntil) {
  // Ensure we are in a thread context, meaning we are permitted to sleep.
  ASSERT_NE(get_id(), thread::Id());

  const SystemClock::time_point deadline =
      SystemClock::now() + kRoundedArbitraryDuration;
  sleep_until(deadline);
  EXPECT_GE(SystemClock::now(), deadline);
}

TEST(Sleep, SleepForInC) {
  // Ensure we are in a thread context, meaning we are permitted to sleep.
  ASSERT_NE(get_id(), thread::Id());

  pw_chrono_SystemClock_TimePoint before = pw_chrono_SystemClock_Now();
  pw_this_thread_SleepFor(kRoundedArbitraryDurationInC);
  pw_chrono_SystemClock_TickCount time_elapsed =
      pw_chrono_SystemClock_Now().ticks_since_epoch - before.ticks_since_epoch;
  EXPECT_GE(time_elapsed, kRoundedArbitraryDurationInC);
}

TEST(Sleep, SleepUntilInC) {
  // Ensure we are in a thread context, meaning we are permitted to sleep.
  ASSERT_NE(get_id(), thread::Id());

  pw_chrono_SystemClock_TimePoint deadline;
  deadline.ticks_since_epoch = pw_chrono_SystemClock_Now().ticks_since_epoch +
                               kRoundedArbitraryDurationInC;
  pw_this_thread_CallSleepUntil(deadline);
  EXPECT_GE(pw_chrono_SystemClock_Now().ticks_since_epoch,
            deadline.ticks_since_epoch);
}

}  // namespace
}  // namespace pw::this_thread
