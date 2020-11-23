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

#include "pw_sync/spin_lock.h"

#include "pw_assert/assert.h"
#include "tx_api.h"

namespace pw::sync {

void SpinLock::lock() {
  // In order to be pw::sync::SpinLock compliant, mask the interrupts
  // before attempting to grab the internal spin lock.
  native_type_.saved_interrupt_mask = tx_interrupt_control(TX_INT_DISABLE);

  // This implementation is not set up to support SMP, meaning we cannot
  // deadlock here due to the global interrupt lock, so we crash on recursion
  // on a specific spinlock instead.
  PW_CHECK(!native_type_.locked.load(std::memory_order_relaxed),
           "Recursive SpinLock::lock() detected");

  native_type_.locked.store(true, std::memory_order_relaxed);
}

bool SpinLock::try_lock() {
  // In order to be pw::sync::SpinLock compliant, mask the interrupts
  // before attempting to grab the internal spin lock.
  UINT saved_interrupt_mask = tx_interrupt_control(TX_INT_DISABLE);

  if (native_type_.locked.load(std::memory_order_relaxed)) {
    // Already locked, restore interrupts and bail out.
    tx_interrupt_control(saved_interrupt_mask);
    return false;
  }

  native_type_.saved_interrupt_mask = saved_interrupt_mask;
  native_type_.locked.store(true, std::memory_order_relaxed);
  return true;
}

void SpinLock::unlock() {
  native_type_.locked.store(false, std::memory_order_relaxed);
  tx_interrupt_control(native_type_.saved_interrupt_mask);
}

}  // namespace pw::sync
