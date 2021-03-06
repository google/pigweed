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

#include "pw_sync/counting_semaphore.h"

#include <algorithm>

#include "RTOS.h"
#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_embos/system_clock_constants.h"
#include "pw_interrupt/context.h"

using pw::chrono::SystemClock;

namespace pw::sync {

void CountingSemaphore::release(ptrdiff_t update) {
  for (; update > 0; --update) {
    // There is no API to atomically detect overflow, however debug builds of
    // embOS call OS_Error() internally when overflow is detected for the native
    // token representation. Rather than enter a critical section both due to
    // cost and potential direct use of the native handle, a lazy check is used
    // for debug builds which may not trigger on the initial overflow.
    PW_DCHECK_UINT_LE(OS_GetCSemaValue(&native_type_),
                      CountingSemaphore::max(),
                      "Overflowed counting semaphore.");
    OS_SignalCSema(&native_type_);
  }
}

bool CountingSemaphore::try_acquire_for(SystemClock::duration for_at_least) {
  PW_DCHECK(!interrupt::InInterruptContext());

  // Use non-blocking try_acquire for negative and zero length durations.
  if (for_at_least <= SystemClock::duration::zero()) {
    return try_acquire();
  }

  // On a tick based kernel we cannot tell how far along we are on the current
  // tick, ergo we add one whole tick to the final duration.
  constexpr SystemClock::duration kMaxTimeoutMinusOne =
      pw::chrono::embos::kMaxTimeout - SystemClock::duration(1);
  while (for_at_least > kMaxTimeoutMinusOne) {
    if (OS_WaitCSemaTimed(&native_type_,
                          static_cast<OS_TIME>(kMaxTimeoutMinusOne.count()))) {
      return true;
    }
    for_at_least -= kMaxTimeoutMinusOne;
  }
  return OS_WaitCSemaTimed(&native_type_,
                           static_cast<OS_TIME>(for_at_least.count() + 1));
}

}  // namespace pw::sync
