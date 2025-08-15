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
#pragma once

#include "pw_async2/context.h"
#include "pw_async2/waker.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace codelab {

// Represents the coin slot hardware for a vending machine.
class CoinSlot {
 public:
  constexpr CoinSlot() : coins_deposited_(0) {}

  // Pends until the coins have been deposited. Returns the number of coins
  // received.
  //
  // May only be called by one task.
  pw::async2::Poll<unsigned> Pend(pw::async2::Context& context);

  // Report that a coin was received by the coin slot. Typically called from the
  // coin slot ISR.
  void Deposit();

 private:
  pw::sync::InterruptSpinLock lock_;

  // The number of coins deposited since the last Pend() call.
  unsigned coins_deposited_ PW_GUARDED_BY(lock_);

  // Wakes a task that called Pend() when a coin arrives.
  pw::async2::Waker waker_ PW_GUARDED_BY(lock_);
};

}  // namespace codelab
