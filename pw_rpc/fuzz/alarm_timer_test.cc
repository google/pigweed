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

#include "gtest/gtest.h"
#include "pw_sync/binary_semaphore.h"

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
  sync::BinarySemaphore sem;
  AlarmTimer timer([&sem](chrono::SystemClock::time_point) { sem.release(); });
  timer.Start(50ms);
  for (size_t i = 0; i < 10; ++i) {
    timer.Restart();
    EXPECT_FALSE(sem.try_acquire_for(chrono::SystemClock::for_at_least(10us)));
  }
  sem.acquire();
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
