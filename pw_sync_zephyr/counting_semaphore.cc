// Copyright 2025 The Pigweed Authors
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

#include <zephyr/kernel.h>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"

namespace pw::sync {

void CountingSemaphore::release(ptrdiff_t update) {
  for (; update > 0; --update) {
    k_sem_give(&native_type_);
  }
}

bool CountingSemaphore::try_acquire_for(chrono::SystemClock::duration timeout) {
  // Enforce the pw::sync::CountingSemaphore IRQ contract.
  PW_DCHECK(!interrupt::InInterruptContext());

  // Use non-blocking try_acquire for negative and zero length durations.
  if (timeout <= chrono::SystemClock::duration::zero()) {
    return try_acquire();
  }

#ifndef CONFIG_TIMEOUT_64BIT
  constexpr chrono::SystemClock::duration kMaxTimeoutMinusOne =
      chrono::SystemClock::duration(K_FOREVER.ticks - 1);

  while (timeout > kMaxTimeoutMinusOne) {
    if (k_sem_take(&native_type_, K_TICKS(kMaxTimeoutMinusOne.count())) == 0) {
      return true;
    }
    timeout -= kMaxTimeoutMinusOne;
  }
#endif  // CONFIG_TIMEOUT_64BIT

  return k_sem_take(&native_type_, K_TICKS(timeout.count())) == 0;
}

}  // namespace pw::sync
