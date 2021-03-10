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

#include "pw_sync/counting_semaphore.h"

#include <algorithm>

#include "pw_assert/assert.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_threadx/system_clock_constants.h"
#include "pw_interrupt/context.h"
#include "tx_api.h"

using pw::chrono::SystemClock;

namespace pw::sync {

bool CountingSemaphore::try_acquire_for(SystemClock::duration for_at_least) {
  // Enforce the pw::sync::CountingSemaphore IRQ contract.
  PW_DCHECK(!interrupt::InInterruptContext());

  // Use non-blocking try_acquire for negative and zero length durations.
  if (for_at_least <= SystemClock::duration::zero()) {
    return try_acquire();
  }

  // On a tick based kernel we cannot tell how far along we are on the current
  // tick, ergo we add one whole tick to the final duration.
  constexpr SystemClock::duration kMaxTimeoutMinusOne =
      pw::chrono::threadx::kMaxTimeout - SystemClock::duration(1);
  while (for_at_least > kMaxTimeoutMinusOne) {
    const UINT result = tx_semaphore_get(
        &native_type_, static_cast<ULONG>(kMaxTimeoutMinusOne.count()));
    if (result != TX_NO_INSTANCE) {
      // If we didn't time out (TX_NO_INSTANCE), then we should have succeeded.
      PW_CHECK_UINT_EQ(TX_SUCCESS, result);
      return true;
    }
    for_at_least -= kMaxTimeoutMinusOne;
  }
  const UINT result = tx_semaphore_get(
      &native_type_, static_cast<ULONG>(for_at_least.count() + 1));
  if (result == TX_NO_INSTANCE) {
    return false;  // We timed out, there's still no token.
  }
  PW_CHECK_UINT_EQ(TX_SUCCESS, result);
  return true;
}

}  // namespace pw::sync
