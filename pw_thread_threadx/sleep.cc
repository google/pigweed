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

#include "pw_assert/assert.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_threadx/system_clock_constants.h"
#include "pw_thread/id.h"
#include "tx_api.h"

using pw::chrono::threadx::kMaxTimeout;

namespace pw::this_thread {

void sleep_for(chrono::SystemClock::duration for_at_least) {
  PW_DCHECK(get_id() != thread::Id());

  // Clamp negative durations to be 0 which maps to non-blocking.
  for_at_least = std::max(for_at_least, chrono::SystemClock::duration::zero());

  // The pw::sleep_{for,until} API contract is to yield if we attempt to sleep
  // for a duration of 0. ThreadX's tx_thread_sleep does a no-op if 0 is passed,
  // ergo we explicitly check for the yield condition if the duration is 0.
  if (for_at_least == chrono::SystemClock::duration::zero()) {
    tx_thread_relinquish();  // Direct API is used to reduce overhead.
    return;
  }

  while (for_at_least > kMaxTimeout) {
    const UINT result = tx_thread_sleep(kMaxTimeout.count());
    PW_CHECK_UINT_EQ(TX_SUCCESS, result);
    for_at_least -= kMaxTimeout;
  }
  const UINT result = tx_thread_sleep(for_at_least.count());
  PW_CHECK_UINT_EQ(TX_SUCCESS, result);
}

}  // namespace pw::this_thread
