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

#include "pw_rpc/fuzz/alarm_timer.h"

#include <chrono>

#include "pw_sync/binary_semaphore.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc::fuzz {
namespace {

using namespace std::chrono_literals;

TEST(AlarmTimerTest, Start) {
  sync::BinarySemaphore sem;
  AlarmTimer timer([&sem](chrono::SystemClock::time_point) { sem.release(); });
  timer.Start(10ms);
  sem.acquire();
}

TEST(AlarmTimerTest, Restart) {
  sync::BinarySemaphore final_sem;
  sync::BinarySemaphore kick_sem;
  constexpr auto kTimerDuration = 200ms;
  constexpr auto kTimerKickDuration = 10ms;
  constexpr size_t kNumRestarts = 10;
  static_assert(kTimerKickDuration < kTimerDuration);

  AlarmTimer timer(
      [&final_sem](chrono::SystemClock::time_point) { final_sem.release(); });
  AlarmTimer timer_kicker(
      [&kick_sem](chrono::SystemClock::time_point) { kick_sem.release(); });

  timer.Start(chrono::SystemClock::for_at_least(kTimerDuration));

  bool acquired = false;
  const auto start = chrono::SystemClock::now();
  for (size_t i = 0; i < kNumRestarts; ++i) {
    // Be overly aggressive with restarting the timer, the point is to ensure
    // that it doesn't time out when restareted. Since this tests timings, it
    // inherrently is very prone to flake in some environments (e.g. heavy load
    // on a Windows machine).
    timer.Restart();
    timer_kicker.Start(chrono::SystemClock::for_at_least(kTimerKickDuration));
    timer.Restart();
    kick_sem.acquire();
    timer.Restart();

    acquired = final_sem.try_acquire();
    EXPECT_FALSE(acquired);
    if (acquired) {
      break;
    }
  }

  if (!acquired) {
    final_sem.acquire();
  }
  auto end = chrono::SystemClock::now();
  EXPECT_GT(end - start, kTimerKickDuration * kNumRestarts + kTimerDuration);
}

TEST(AlarmTimerTest, Cancel) {
  sync::BinarySemaphore sem;
  AlarmTimer timer([&sem](chrono::SystemClock::time_point) { sem.release(); });
  timer.Start(50ms);
  timer.Cancel();
  EXPECT_FALSE(sem.try_acquire_for(chrono::SystemClock::for_at_least(100us)));
}

TEST(AlarmTimerTest, Destroy) {
  sync::BinarySemaphore sem;
  {
    AlarmTimer timer(
        [&sem](chrono::SystemClock::time_point) { sem.release(); });
    timer.Start(50ms);
  }
  EXPECT_FALSE(sem.try_acquire_for(chrono::SystemClock::for_at_least(100us)));
}

}  // namespace
}  // namespace pw::rpc::fuzz
