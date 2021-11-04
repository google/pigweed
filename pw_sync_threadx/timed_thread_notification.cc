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

#include "pw_sync/timed_thread_notification.h"

#include <mutex>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_threadx/system_clock_constants.h"
#include "pw_interrupt/context.h"
#include "tx_api.h"

namespace pw::sync {
namespace {

using pw::chrono::SystemClock;

}  // namespace

bool TimedThreadNotification::try_acquire_for(SystemClock::duration timeout) {
  PW_DCHECK(!interrupt::InInterruptContext());
  PW_DCHECK(native_handle().blocked_thread == nullptr);
  {
    std::lock_guard lock(backend::thread_notification_isl);
    const bool notified = native_handle().notified;
    // Don't block for negative or zero length durations.
    if (notified || timeout <= SystemClock::duration::zero()) {
      native_handle().notified = false;
      return notified;
    }
    // Not notified yet, register the thread for a one-time notification.
    native_handle().blocked_thread = tx_thread_identify();
  }

  const bool notified = [&]() {
    // On a tick based kernel we cannot tell how far along we are on the current
    // tick, ergo we add one whole tick to the final duration.
    constexpr SystemClock::duration kMaxTimeoutMinusOne =
        pw::chrono::threadx::kMaxTimeout - SystemClock::duration(1);
    // In case the timeout is too long for us to express through the native
    // ThreadX API, we repeatedly wait with shorter durations.
    while (timeout > kMaxTimeoutMinusOne) {
      const UINT result =
          tx_thread_sleep(static_cast<ULONG>(kMaxTimeoutMinusOne.count()));
      if (result != TX_SUCCESS) {
        PW_CHECK_UINT_EQ(TX_WAIT_ABORTED, result);
        return true;
      }
      timeout -= kMaxTimeoutMinusOne;
    }

    const UINT result =
        tx_thread_sleep(static_cast<ULONG>(timeout.count() + 1));
    if (result == TX_SUCCESS) {
      return false;
    }
    PW_CHECK_UINT_EQ(TX_WAIT_ABORTED, result);
    return true;
  }();

  {
    std::lock_guard lock(backend::thread_notification_isl);
    if (notified) {
      // Note that this may hide another notification, however this is
      // considered form of notification saturation just like as if this
      // happened before acquire() was invoked.
      native_handle().notified = false;
      // The thread pointer was cleared by the notifier.
    } else {
      // Note that we do NOT want to clear the notified value so the next call
      // can detect the notification which came after we timed out but before
      // this critical section.
      //
      // However, we do need to clear the thread pointer if we weren't notified.
      native_handle().blocked_thread = nullptr;
    }
  }
  return notified;
}

}  // namespace pw::sync
