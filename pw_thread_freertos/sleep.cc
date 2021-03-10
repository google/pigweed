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

#include "pw_thread/sleep.h"

#include <algorithm>

#include "FreeRTOS.h"
#include "pw_assert/assert.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_freertos/system_clock_constants.h"
#include "pw_thread/id.h"
#include "task.h"

using pw::chrono::SystemClock;

namespace pw::this_thread {

void sleep_for(SystemClock::duration for_at_least) {
  PW_DCHECK(get_id() != thread::Id());

  // Yield for negative and zero length durations.
  if (for_at_least <= SystemClock::duration::zero()) {
    taskYIELD();
    return;
  }

  // On a tick based kernel we cannot tell how far along we are on the current
  // tick, ergo we add one whole tick to the final duration.
  constexpr SystemClock::duration kMaxTimeoutMinusOne =
      pw::chrono::freertos::kMaxTimeout - SystemClock::duration(1);
  while (for_at_least > kMaxTimeoutMinusOne) {
    vTaskDelay(static_cast<TickType_t>(kMaxTimeoutMinusOne.count()));
    for_at_least -= kMaxTimeoutMinusOne;
  }
  vTaskDelay(static_cast<TickType_t>(for_at_least.count() + 1));
}

}  // namespace pw::this_thread
