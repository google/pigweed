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

#include "coin_slot.h"
#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/task.h"
#include "pw_async2/waker.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace codelab {

class Keypad {
 public:
  constexpr Keypad() : key_pressed_(kNone) {}

  // Pends until a key has been pressed, returning the key number.
  //
  // May only be called by one task.
  pw::async2::Poll<int> Pend(pw::async2::Context& cx);

  // Record a key press. Typically called from the keypad ISR.
  void Press(int key);

 private:
  // A special internal value to indicate no keypad button has yet been
  // pressed.
  static constexpr int kNone = -1;

  pw::sync::InterruptSpinLock lock_;
  int key_pressed_ PW_GUARDED_BY(lock_);
  pw::async2::Waker waker_;  // No guard needed!
};

// The main task that drives the vending machine.
class VendingMachineTask : public pw::async2::Task {
 public:
  VendingMachineTask(CoinSlot& coin_slot, Keypad& keypad)
      : pw::async2::Task(PW_ASYNC_TASK_NAME("VendingMachineTask")),
        coin_slot_(coin_slot),
        displayed_welcome_message_(false),
        keypad_(keypad),
        coins_inserted_(0) {}

 private:
  // This is the core of the asynchronous task. The dispatcher calls this method
  // to give the task a chance to do work.
  pw::async2::Poll<> DoPend(pw::async2::Context& cx) override;

  CoinSlot& coin_slot_;
  bool displayed_welcome_message_;
  Keypad& keypad_;
  unsigned coins_inserted_;
};

}  // namespace codelab
