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

#include <cmath>

#include "gtest/gtest.h"
#include "pico/critical_section.h"
#include "pw_chrono/system_clock.h"

namespace pw::chrono::rp2040 {
namespace {

static_assert(pw::chrono::SystemClock::is_free_running == true);

TEST(ClockProperties, IsFreeRunning) {
  // Enter a critical section.
  critical_section state;
  critical_section_init(&state);
  critical_section_enter_blocking(&state);

  // Check initial clock value.
  auto start = pw::chrono::SystemClock::now();

  // Do some work.
  volatile float num = 4;
  for (int i = 0; i < 100; i++) {
    float tmp = std::pow(num, 4);
    num = tmp;
  }

  // Check final clock value.
  auto end = pw::chrono::SystemClock::now();

  // Exit critical section.
  critical_section_exit(&state);

  EXPECT_GT(num, 4);
  EXPECT_GT(end, start);
}

}  // namespace
}  // namespace pw::chrono::rp2040
