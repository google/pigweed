// Copyright 2021 The Pigweed Authors
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

#include "RTOS.h"
#include "pw_assert/assert.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_embos/system_clock_constants.h"
#include "pw_thread/id.h"

using pw::chrono::embos::kMaxTimeout;

namespace pw::this_thread {

void sleep_for(chrono::SystemClock::duration for_at_least) {
  PW_DCHECK(get_id() != thread::Id());

  // Clamp negative durations to be 0 which maps to non-blocking.
  for_at_least = std::max(for_at_least, chrono::SystemClock::duration::zero());

  // The pw::sleep_{for,until} API contract is to yield if we attempt to sleep
  // for a duration of 0. The embOS delay does not explicitly yield if 0 is
  // passed, ergo we explicitly check for the yield condition.
  if (for_at_least == chrono::SystemClock::duration::zero()) {
    OS_Yield();  // Direct API is used to reduce overhead.
    return;
  }

  while (for_at_least > kMaxTimeout) {
    OS_Delay(kMaxTimeout.count());
    for_at_least -= kMaxTimeout;
  }
  OS_Delay(for_at_least.count());
}

}  // namespace pw::this_thread
