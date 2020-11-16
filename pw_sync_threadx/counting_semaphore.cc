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
using pw::chrono::threadx::kMaxTimeout;

namespace pw::sync {

bool CountingSemaphore::try_acquire_for(SystemClock::duration for_at_least) {
  // Enforce the pw::sync::CountingSemaphore IRQ contract.
  PW_DCHECK(!interrupt::InInterruptContext());

  // Clamp negative durations to be 0 which maps to non-blocking.
  for_at_least = std::max(for_at_least, SystemClock::duration::zero());

  while (for_at_least > kMaxTimeout) {
    const UINT result = tx_semaphore_get(&native_type_, kMaxTimeout.count());
    if (result != TX_NO_INSTANCE) {
      // If we didn't time out (TX_NO_INSTANCE), then we should have succeeded.
      PW_CHECK_UINT_EQ(TX_SUCCESS, result);
      return true;
    }
    for_at_least -= kMaxTimeout;
  }
  const UINT result = tx_semaphore_get(&native_type_, for_at_least.count());
  if (result == TX_NO_INSTANCE) {
    return false;  // We timed out, there's still no token.
  }
  PW_CHECK_UINT_EQ(TX_SUCCESS, result);
  return true;
}

}  // namespace pw::sync
