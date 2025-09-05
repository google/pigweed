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

#include <optional>

#include "coin_slot.h"
#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/system_time_provider.h"
#include "pw_async2/task.h"
#include "pw_async2/time_provider.h"
#include "pw_async2/waker.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/inline_async_deque.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace codelab {

using DispenseRequestQueue = pw::InlineAsyncDeque<int, 1>;
using DispenseResponseQueue = pw::InlineAsyncDeque<bool, 1>;

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
  pw::async2::Waker waker_ PW_GUARDED_BY(lock_);
};

class ItemDropSensor {
 public:
  constexpr ItemDropSensor() = default;

  // Pends until theitem drop sensor triggers.
  pw::async2::Poll<> Pend(pw::async2::Context& cx);

  // Records an item drop event. Typically called from the drop sensor ISR.
  void Drop();

 private:
  pw::sync::InterruptSpinLock lock_;
  bool drop_detected_ PW_GUARDED_BY(lock_) = false;
  pw::async2::Waker waker_ PW_GUARDED_BY(lock_);
};

// Does this belong in pw_async2?
inline pw::sync::InterruptSpinLock& codelab_sender_receiver_lock() {
  static pw::NoDestructor<pw::sync::InterruptSpinLock> lock;
  return *lock;
}

// The main task that drives the vending machine.
class VendingMachineTask : public pw::async2::Task {
 public:
  VendingMachineTask(CoinSlot& coin_slot,
                     Keypad& keypad,
                     DispenseRequestQueue& dispense_requests,
                     DispenseResponseQueue& dispense_responses)
      : pw::async2::Task(PW_ASYNC_TASK_NAME("VendingMachineTask")),
        coin_slot_(coin_slot),
        keypad_(keypad),
        dispense_requests_(dispense_requests),
        dispense_responses_(dispense_responses),
        state_(kWelcome),
        coins_inserted_(0) {}

 private:
  enum State {
    kWelcome,
    kAwaitingPayment,
    kAwaitingSelection,
    kAwaitingDispenseIdle,
    kAwaitingDispense,
  };

  enum Input {
    kNone,
    kCoinInserted,
    kKeyPressed,
  };

  // This is the core of the asynchronous task. The dispatcher calls this method
  // to give the task a chance to do work.
  pw::async2::Poll<> DoPend(pw::async2::Context& cx) override;

  // Waits for either an inserted coin or keypress to occur, updating either
  // `coins_inserted_` or `selected_item_` accordingly.
  pw::async2::Poll<Input> PendInput(pw::async2::Context& cx);

  CoinSlot& coin_slot_;
  Keypad& keypad_;
  DispenseRequestQueue& dispense_requests_;
  DispenseResponseQueue& dispense_responses_;
  State state_;
  unsigned coins_inserted_;
  std::optional<int> selected_item_;
};

class DispenserTask : public pw::async2::Task {
 public:
  DispenserTask(ItemDropSensor& item_drop_sensor,
                DispenseRequestQueue& dispense_requests,
                DispenseResponseQueue& dispense_responses)

      : item_drop_sensor_(item_drop_sensor),
        dispense_requests_(dispense_requests),
        dispense_responses_(dispense_responses),
        state_{kIdle} {}

 private:
  static constexpr auto kDispenseTimeout = std::chrono::seconds(5);

  enum State {
    kIdle,
    kDispensing,
    kReportDispenseResult,
  };

  pw::async2::Poll<> DoPend(pw::async2::Context& cx) override;

  ItemDropSensor& item_drop_sensor_;
  DispenseRequestQueue& dispense_requests_;
  DispenseResponseQueue& dispense_responses_;
  pw::async2::TimeFuture<pw::chrono::SystemClock> timeout_future_;
  std::optional<int> current_item_;
  State state_;
};

}  // namespace codelab
