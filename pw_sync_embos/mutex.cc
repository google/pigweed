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

#include "pw_sync/mutex.h"

#include <algorithm>

#include "RTOS.h"
#include "pw_assert/assert.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_embos/system_clock_constants.h"
#include "pw_interrupt/context.h"

using pw::chrono::SystemClock;
using pw::chrono::embos::kMaxTimeout;

namespace pw::sync {

bool Mutex::try_lock_for(SystemClock::duration for_at_least) {
  PW_DCHECK(!interrupt::InInterruptContext());

  // Clamp negative durations to be 0 which maps to non-blocking.
  for_at_least = std::max(for_at_least, SystemClock::duration::zero());

  while (for_at_least > kMaxTimeout) {
    const int lock_count = OS_UseTimed(&native_type_, kMaxTimeout.count());
    if (lock_count != 0) {
      PW_CHECK_UINT_EQ(1, lock_count, "Recursive locking is not permitted");
      return true;
    }
    for_at_least -= kMaxTimeout;
  }
  const int lock_count = OS_UseTimed(&native_type_, for_at_least.count());
  PW_CHECK_UINT_LE(1, lock_count, "Recursive locking is not permitted");
  return lock_count == 1;
}

}  // namespace pw::sync
