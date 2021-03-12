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

#include "pw_sync/interrupt_spin_lock.h"

#include "pw_assert/assert.h"
#include "pw_interrupt/context.h"
#include "task.h"

namespace pw::sync {

void InterruptSpinLock::lock() {
  if (interrupt::InInterruptContext()) {
    native_type_.saved_interrupt_mask = taskENTER_CRITICAL_FROM_ISR();
  } else {  // Task context
    taskENTER_CRITICAL();
  }
  // We can't deadlock here so crash instead.
  PW_CHECK(!native_type_.locked.load(std::memory_order_relaxed),
           "Recursive InterruptSpinLock::lock() detected");
  native_type_.locked.store(true, std::memory_order_relaxed);
}

bool InterruptSpinLock::try_lock() {
  if (interrupt::InInterruptContext()) {
    UBaseType_t saved_interrupt_mask = taskENTER_CRITICAL_FROM_ISR();
    if (native_type_.locked.load(std::memory_order_relaxed)) {
      // Already locked, restore interrupts and bail out.
      taskEXIT_CRITICAL_FROM_ISR(saved_interrupt_mask);
      return false;
    }
    native_type_.saved_interrupt_mask = saved_interrupt_mask;
  } else {  // Task context
    taskENTER_CRITICAL();
    if (native_type_.locked.load(std::memory_order_relaxed)) {
      // ALready locked, restore interrupts and bail out.
      taskEXIT_CRITICAL();
      return false;
    }
  }
  native_type_.locked.store(true, std::memory_order_relaxed);
  return true;
}

void InterruptSpinLock::unlock() {
  native_type_.locked.store(false, std::memory_order_relaxed);
  if (interrupt::InInterruptContext()) {
    taskEXIT_CRITICAL_FROM_ISR(native_type_.saved_interrupt_mask);
  } else {  // Task context
    taskEXIT_CRITICAL();
  }
}

}  // namespace pw::sync
