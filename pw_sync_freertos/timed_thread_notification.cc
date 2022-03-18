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

#include "pw_sync/timed_thread_notification.h"

#include "FreeRTOS.h"
#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono_freertos/system_clock_constants.h"
#include "pw_interrupt/context.h"
#include "pw_sync_freertos/config.h"
#include "task.h"

using pw::chrono::SystemClock;

namespace pw::sync {
namespace {

BaseType_t WaitForNotification(TickType_t xTicksToWait) {
#ifdef configTASK_NOTIFICATION_ARRAY_ENTRIES
  return xTaskNotifyWaitIndexed(
      pw::sync::freertos::config::kThreadNotificationIndex,
      0,        // Clear no bits on entry.
      0,        // Clear no bits on exit.
      nullptr,  // Don't care about the notification value.
      xTicksToWait);
#else   // !configTASK_NOTIFICATION_ARRAY_ENTRIES
  return xTaskNotifyWait(0,        // Clear no bits on entry.
                         0,        // Clear no bits on exit.
                         nullptr,  // Don't care about the notification value.
                         xTicksToWait);
#endif  // configTASK_NOTIFICATION_ARRAY_ENTRIES
}

}  // namespace

bool TimedThreadNotification::try_acquire_for(SystemClock::duration timeout) {
  // Enforce the pw::sync::TImedThreadNotification IRQ contract.
  PW_DCHECK(!interrupt::InInterruptContext());

  // Enforce that only a single thread can block at a time.
  PW_DCHECK(native_handle().blocked_thread == nullptr);

  // Ensure that no one forgot to clean up nor corrupted the task notification
  // state in the TCB.
  PW_DCHECK(xTaskNotifyStateClear(nullptr) == pdFALSE);

  taskENTER_CRITICAL();
  {
    const bool notified = native_handle().notified;
    // Don't block for negative or zero length durations.
    if (notified || (timeout <= SystemClock::duration::zero())) {
      native_handle().notified = false;
      taskEXIT_CRITICAL();
      return notified;
    }
    // Not notified yet, set the task handle for a one-time notification.
    native_handle().blocked_thread = xTaskGetCurrentTaskHandle();
  }
  taskEXIT_CRITICAL();

  const bool notified = [&]() {
    // In case the timeout is too long for us to express through the native
    // FreeRTOS API, we repeatedly wait with shorter durations. Note that on a
    // tick based kernel we cannot tell how far along we are on the current
    // tick, ergo we add one whole tick to the final duration. However, this
    // also means that the loop must ensure that timeout + 1 is less than the
    // max timeout.
    constexpr SystemClock::duration kMaxTimeoutMinusOne =
        pw::chrono::freertos::kMaxTimeout - SystemClock::duration(1);
    // In case the timeout is too long for us to express through the native
    // FreeRTOS API, we repeatedly wait with shorter durations.
    while (timeout > kMaxTimeoutMinusOne) {
      if (WaitForNotification(
              static_cast<TickType_t>(kMaxTimeoutMinusOne.count())) == pdTRUE) {
        return true;
      }
      timeout -= kMaxTimeoutMinusOne;
    }

    // On a tick based kernel we cannot tell how far along we are on the current
    // tick, ergo we add one whole tick to the final duration.
    return WaitForNotification(static_cast<TickType_t>(timeout.count() + 1)) ==
           pdTRUE;
  }();

  taskENTER_CRITICAL();
  if (notified) {
    // Note that this may hide another notification, however this is considered
    // a form of notification saturation just like as if this happened before
    // acquire() was invoked.
    native_handle().notified = false;
    // The task handle and notification state were cleared by the notifier.
  } else {
    // Note that we do NOT want to clear the notified value so the next call
    // can detect the notification which came after we timed out but before this
    // critical section.
    //
    // However, we do need to clear the task handle if we weren't notified and
    // the notification state in case we were notified to ensure we can block
    // in the future.
    native_handle().blocked_thread = nullptr;
#ifdef configTASK_NOTIFICATION_ARRAY_ENTRIES
    xTaskNotifyStateClearIndexed(
        nullptr, pw::sync::freertos::config::kThreadNotificationIndex);
#else   // !configTASK_NOTIFICATION_ARRAY_ENTRIES
    xTaskNotifyStateClear(nullptr);
#endif  // configTASK_NOTIFICATION_ARRAY_ENTRIES
  }
  taskEXIT_CRITICAL();
  return notified;
}

}  // namespace pw::sync
