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
#include "pw_preprocessor/util.h"

namespace pw::chrono {
namespace {

extern "C" {

// Functions defined in system_clock_facade_test_c.c which call the API from C.
pw_chrono_SystemClock_TimePoint pw_chrono_SystemClock_CallNow();
pw_chrono_SystemClock_TickCount pw_chrono_SystemClock_CallTimeDelta(
    pw_chrono_SystemClock_TimePoint last_time,
    pw_chrono_SystemClock_TimePoint current_time);

int32_t pw_chrono_SystemClock_PeriodSeconds_CallNumerator();
int32_t pw_chrono_SystemClock_PeriodSeconds_CallDenominator();

pw_chrono_SystemClock_Nanoseconds
pw_chrono_SystemClock_CallTickCountToNsTruncate(
    pw_chrono_SystemClock_TickCount ticks);

}  // extern "C"

// While testing that the clock ticks (i.e. moves forward) we want to ensure a
// failure can be reported instead of deadlocking the test until it passes.
// Given that there isn't really a good heuristic for this we instead make some
// wild assumptions to bound the maximum busy loop iterations.
// - Assume our clock is < 6Ghz
// - Assume we can check the clock in a single cycle
// - Wait for up to 1/10th of a second @ 6Ghz, this may be a long period on a
//   slower (i.e. real) machine.
constexpr uint64_t kMaxIterations = 6'000'000'000 / 10;

TEST(SystemClock, Now) {
  const SystemClock::time_point start_time = SystemClock::now();
  // Verify the clock moves forward.
  bool clock_moved_forward = false;
  for (uint64_t i = 0; i < kMaxIterations; ++i) {
    if (SystemClock::now() > start_time) {
      clock_moved_forward = true;
      break;
    }
  }
  EXPECT_TRUE(clock_moved_forward);
}

TEST(VirtualSystemClock, Now) {
  auto& clock = VirtualSystemClock::RealClock();
  const SystemClock::time_point start_time = clock.now();
  // Verify the clock moves forward.
  bool clock_moved_forward = false;
  for (uint64_t i = 0; i < kMaxIterations; ++i) {
    if (clock.now() > start_time) {
      clock_moved_forward = true;
      break;
    }
  }
  EXPECT_TRUE(clock_moved_forward);
}

TEST(SystemClock, NowInC) {
  const pw_chrono_SystemClock_TimePoint start_time =
      pw_chrono_SystemClock_CallNow();
  // Verify the clock moves forward.
  bool clock_moved_forward = false;
  for (uint64_t i = 0; i < kMaxIterations; ++i) {
    if (pw_chrono_SystemClock_CallNow().ticks_since_epoch >
        start_time.ticks_since_epoch) {
      clock_moved_forward = true;
      break;
    }
  }
  EXPECT_TRUE(clock_moved_forward);
}

TEST(SystemClock, TimeDeltaInC) {
  const pw_chrono_SystemClock_TimePoint first = pw_chrono_SystemClock_CallNow();
  const pw_chrono_SystemClock_TimePoint last = pw_chrono_SystemClock_CallNow();
  static_assert(SystemClock::is_monotonic);
  EXPECT_GE(0, pw_chrono_SystemClock_CallTimeDelta(last, first));
}

TEST(SystemClock, PeriodRatioInC) {
  EXPECT_EQ(SystemClock::period::num,
            pw_chrono_SystemClock_PeriodSeconds_CallNumerator());
  EXPECT_EQ(SystemClock::period::den,
            pw_chrono_SystemClock_PeriodSeconds_CallDenominator());
}

TEST(SystemClock, DurationCastInC) {
  static constexpr auto kArbitraryPeriod = std::chrono::hours(42);
  // We can't control the SystemClock's period configuration, so just in case
  // 42 hours cannot be accurately expressed in integer ticks, round the
  // duration w/ duration_cast.
  static constexpr auto kRoundedArbitraryDuration =
      std::chrono::duration_cast<SystemClock::duration>(kArbitraryPeriod);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::nanoseconds>(
                kRoundedArbitraryDuration)
                .count(),
            pw_chrono_SystemClock_CallTickCountToNsTruncate(
                kRoundedArbitraryDuration.count()));
}

}  // namespace
}  // namespace pw::chrono
