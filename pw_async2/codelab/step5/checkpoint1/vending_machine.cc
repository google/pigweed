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

#include "vending_machine.h"

#include <mutex>

#include "hardware.h"
#include "pw_async2/pendable.h"
#include "pw_async2/select.h"
#include "pw_async2/try.h"
#include "pw_log/log.h"

namespace codelab {

pw::async2::Poll<int> Keypad::Pend(pw::async2::Context& cx) {
  std::lock_guard lock(lock_);
  int key = std::exchange(key_pressed_, kNone);
  if (key != kNone) {
    return key;
  }
  PW_ASYNC_STORE_WAKER(cx, waker_, "keypad press");
  return pw::async2::Pending();
}

void Keypad::Press(int key) {
  std::lock_guard lock(lock_);
  key_pressed_ = key;
  std::move(waker_).Wake();
}

pw::async2::Poll<VendingMachineTask::Input> VendingMachineTask::PendInput(
    pw::async2::Context& cx) {
  Input input = kNone;
  selected_item_ = std::nullopt;

  PW_TRY_READY_ASSIGN(
      auto result,
      pw::async2::Select(cx,
                         pw::async2::PendableFor<&CoinSlot::Pend>(coin_slot_),
                         pw::async2::PendableFor<&Keypad::Pend>(keypad_)));
  pw::async2::VisitSelectResult(
      result,
      [](pw::async2::AllPendablesCompleted) {},
      [&](unsigned coins) {
        coins_inserted_ += coins;
        input = kCoinInserted;
      },
      [&](int key) {
        selected_item_ = key;
        input = kKeyPressed;
      });

  return input;
}

pw::async2::Poll<> VendingMachineTask::DoPend(pw::async2::Context& cx) {
  while (true) {
    switch (state_) {
      case kWelcome: {
        PW_LOG_INFO("Welcome to the Pigweed Vending Machine!");
        PW_LOG_INFO("Please insert a coin.");
        state_ = kAwaitingPayment;
        break;
      }

      case kAwaitingPayment: {
        PW_TRY_READY_ASSIGN(Input input, PendInput(cx));
        switch (input) {
          case kCoinInserted:
            PW_LOG_INFO("Received %u coin%s.",
                        coins_inserted_,
                        coins_inserted_ != 1 ? "s" : "");
            if (coins_inserted_ > 0) {
              PW_LOG_INFO("Please press a keypad key.");
              state_ = kAwaitingSelection;
            }
            break;
          case kKeyPressed:
            PW_LOG_ERROR("Please insert a coin before making a selection.");
            break;
          case kNone:
            break;
        }
        break;
      }

      case kAwaitingSelection: {
        PW_TRY_READY_ASSIGN(Input input, PendInput(cx));
        switch (input) {
          case kCoinInserted:
            PW_LOG_INFO("Received a coin. Your balance is currently %u coins.",
                        coins_inserted_);
            PW_LOG_INFO("Press a keypad key to select an item.");
            break;
          case kKeyPressed:
            if (!selected_item_.has_value()) {
              state_ = kAwaitingSelection;
              continue;
            }
            PW_LOG_INFO("Keypad %d was pressed. Dispensing an item.",
                        selected_item_.value());
            // Pay for the item.
            coins_inserted_ = 0;
            state_ = kAwaitingDispenseIdle;
            break;
          case kNone:
            break;
        }
        break;
      }

      case kAwaitingDispenseIdle: {
        PW_TRY_READY(dispense_requests_.PendHasSpace(cx));
        dispense_requests_.push(*selected_item_);
        state_ = kAwaitingDispense;
        break;
      }

      case kAwaitingDispense: {
        PW_TRY_READY(dispense_responses_.PendNotEmpty(cx));
        const bool dispensed = dispense_responses_.front();
        dispense_responses_.pop();

        if (dispensed) {
          // Accept the inserted money as payment
          PW_LOG_INFO("Dispense succeeded. Thanks for your purchase!");
          coins_inserted_ = 0;
          state_ = kWelcome;
        } else {
          PW_LOG_INFO("Dispense failed. Choose another selection.");
          state_ = kAwaitingSelection;
        }
        break;
      }
    }
  }

  PW_UNREACHABLE;
}

pw::async2::Poll<> DispenserTask::DoPend(pw::async2::Context& cx) {
  PW_LOG_INFO("Dispenser task awake");
  while (true) {
    switch (state_) {
      case kIdle: {
        // Wait until a purchase is made.
        PW_TRY_READY(dispense_requests_.PendNotEmpty(cx));

        // Clear any previously latched item drops.
        item_drop_sensor_.Clear();

        // Start the motor to dispense the requested item.
        SetDispenserMotorState(dispense_requests_.front(), MotorState::kOn);

        state_ = kDispensing;
        break;
      }
      case kDispensing: {
        // Wait for the item to drop.
        PW_TRY_READY(item_drop_sensor_.Pend(cx));

        // Finished with this dispense request.
        SetDispenserMotorState(dispense_requests_.front(), MotorState::kOff);
        // Remove the last dispense request from the queue.
        dispense_requests_.pop();

        state_ = kReportDispenseSuccess;
        break;
      }
      case kReportDispenseSuccess:
        // Wait for the response queue to have space.
        PW_TRY_READY(dispense_responses_.PendHasSpace(cx));

        // Notify the vending task that an item was successfully dispensed.
        dispense_responses_.push(true);

        state_ = kIdle;
        break;
    }
  }
}

}  // namespace codelab
