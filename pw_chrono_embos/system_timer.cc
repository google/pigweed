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

#include "pw_chrono/system_timer.h"

#include <mutex>

#include "RTOS.h"
#include "pw_assert/check.h"
#include "pw_chrono_embos/system_clock_constants.h"
#include "pw_interrupt/context.h"

namespace pw::chrono {
namespace {

// Instead of adding targeted locks to each instance, simply use the global
// recursive critical section lock. Note it has to be recursive because a timer
// cannot be started with a time period of 0 and ergo the Invoke* API might
// have to directly invoke the callback.
class RecursiveCriticalSectionLock {
 public:
  void lock() {
    OS_IncDI();            // Mask interrupts.
    OS_SuspendAllTasks();  // Disable task switching.
  }

  void unlock() {
    OS_ResumeAllSuspendedTasks();  // Restore task switching.
    OS_DecRI();                    // Restore interrupts.
  }
};
RecursiveCriticalSectionLock recursive_global_timer_lock;

void HandleTimerCallback(void* void_native_system_timer) {
  // NOTE: embOS invokes all timer callbacks from a single interrupt. Ergo we do
  // not add a unnecessary grab of the recursive_global_timer_lock here.
  PW_DCHECK(interrupt::InInterruptContext(),
            "HandleTimerCallback must be invoked from an interrupt");
  // TODO(ewout): can we potentially skip this as it's always in an interrupt
  // handler and the control API is not interrupt safe?
  std::lock_guard lock(recursive_global_timer_lock);

  backend::NativeSystemTimer& native_type =
      *static_cast<backend::NativeSystemTimer*>(void_native_system_timer);
  const SystemClock::duration time_until_deadline =
      native_type.expiry_deadline - SystemClock::now();
  if (time_until_deadline <= SystemClock::duration::zero()) {
    // We have met the deadline, execute the user's callback.
    native_type.user_callback(native_type.expiry_deadline);
    return;
  }
  const SystemClock::duration period =
      std::min(pw::chrono::embos::kMaxTimeout, time_until_deadline);
  OS_SetTimerPeriodEx(&native_type.tcb, static_cast<OS_TIME>(period.count()));
  OS_StartTimerEx(&native_type.tcb);
}

constexpr OS_TIME kInvalidPeriod = 0;

}  // namespace

SystemTimer::SystemTimer(ExpiryCallback callback)
    : native_type_{.tcb{},
                   .expiry_deadline = SystemClock::time_point(),
                   .user_callback = std::move(callback)} {
  OS_CreateTimerEx(
      &native_type_.tcb, HandleTimerCallback, kInvalidPeriod, &native_type_);
}

SystemTimer::~SystemTimer() {
  // Not threadsafe by design.
  Cancel();
  OS_DeleteTimerEx(&native_type_.tcb);
}

void SystemTimer::InvokeAt(SystemClock::time_point timestamp) {
  std::lock_guard lock(recursive_global_timer_lock);

  // Ensure the timer has been cancelled first.
  Cancel();

  native_type_.expiry_deadline = timestamp;
  const SystemClock::duration time_until_deadline =
      timestamp - SystemClock::now();

  // Timers can only be created with a non-zero period, ergo we must immediately
  // invoke the user's callback if it cannot be deferred by at least a partial
  // tick.
  if (time_until_deadline <= SystemClock::duration::zero()) {
    native_type_.user_callback(timestamp);
    return;
  }

  // Schedule the timer as far out as possible. Note that the timeout might be
  // clamped and it may be rescheduled internally.
  const SystemClock::duration period =
      std::min(pw::chrono::embos::kMaxTimeout, time_until_deadline);
  OS_SetTimerPeriodEx(&native_type_.tcb, static_cast<OS_TIME>(period.count()));
  OS_RetriggerTimerEx(&native_type_.tcb);
}

}  // namespace pw::chrono
