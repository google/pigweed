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

#include "pw_thread/sleep.h"

#include <algorithm>
#include <limits>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_zephyr/system_clock_constants.h"

using pw::chrono::SystemClock;

namespace pw::this_thread {

void sleep_until(SystemClock::time_point wakeup_time) {
  SystemClock::time_point now = chrono::SystemClock::now();

  // Check if the expiration deadline has already passed, yield.
  if (wakeup_time <= now) {
    k_yield();
    return;
  }

  // The maximum amount of time we should sleep for in a single command.
  constexpr chrono::SystemClock::duration kMaxTimeoutMinusOne =
      pw::chrono::zephyr::kMaxTimeout - SystemClock::duration(1);

  while (now < wakeup_time) {
    // Sleep either the full remaining duration or the maximum timout
    k_sleep(Z_TIMEOUT_TICKS(
        std::min((wakeup_time - now).count(), kMaxTimeoutMinusOne.count())));

    // Check how much time has passed, the scheduler can wake us up early.
    now = SystemClock::now();
  }
}

}  // namespace pw::this_thread
