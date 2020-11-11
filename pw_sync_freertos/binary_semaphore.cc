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

#include "pw_sync/binary_semaphore.h"

#include <algorithm>

#include "FreeRTOS.h"
#include "pw_assert/assert.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_freertos/system_clock_constants.h"
#include "pw_interrupt/context.h"
#include "semphr.h"

using pw::chrono::SystemClock;
using pw::chrono::freertos::kMaxTimeout;

namespace pw::sync {
namespace {

static_assert(configSUPPORT_STATIC_ALLOCATION != 0);

}  // namespace

bool BinarySemaphore::try_acquire_for(SystemClock::duration for_at_least) {
  PW_DCHECK(!interrupt::InInterruptContext());

  // Clamp negative durations to be 0 which maps to non-blocking.
  for_at_least = std::max(for_at_least, SystemClock::duration::zero());

  while (for_at_least > kMaxTimeout) {
    if (xSemaphoreTake(native_type_.handle, kMaxTimeout.count()) == pdTRUE) {
      return true;
    }
    for_at_least -= kMaxTimeout;
  }
  return xSemaphoreTake(native_type_.handle, for_at_least.count()) == pdTRUE;
}

}  // namespace pw::sync
