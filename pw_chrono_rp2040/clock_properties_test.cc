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

#include "pico/critical_section.h"
#include "pw_chrono/system_clock.h"
#include "pw_unit_test/framework.h"

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
  auto end = start;

  // Poll pw::chrono::SystemClock::now() until a change is detected. If no
  // change is detected after kMaxAttempts, give up.
  constexpr int kMaxAttempts = 100000;
  for (int i = 0; i < kMaxAttempts; ++i) {
    end = pw::chrono::SystemClock::now();
    if (end != start) {
      break;
    }
  }

  // Exit critical section.
  critical_section_exit(&state);

  EXPECT_GT(end, start);
}

}  // namespace
}  // namespace pw::chrono::rp2040
