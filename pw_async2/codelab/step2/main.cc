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

#include "coin_slot.h"
#include "hardware.h"
#include "pw_async2/dispatcher.h"
#include "vending_machine.h"

namespace {

codelab::CoinSlot coin_slot;

}  // namespace

// Interrupt handler function invoked when the user inserts a coin into the
// vending machine.
void coin_inserted_isr() { coin_slot.Deposit(); }

// Interrupt handler function invoked when the user presses a key on the
// machine's keypad. Receives the value of the pressed key (0-9).
void key_press_isr(int /*key*/) {
  // In Step 3, implement your keypad handler here.
}

// Interrupt handler function invoked to simulate the item drop detector
// detecting confirmation that an item was successfully dispensed from the
// machine.
void item_drop_sensor_isr() {
  // In Step 5 you will uses this as part of a new Dispense task that runs
  // the dispenser motor until an item drops, or you time out on the vend
  // operation.
}

int main() {
  pw::async2::Dispatcher dispatcher;
  codelab::HardwareInit(&dispatcher);

  codelab::VendingMachineTask task(coin_slot);
  dispatcher.Post(task);

  // Run the dispatcher until all tasks are complete. The hardware simulation
  // runs in a background thread and will wake the VendingMachineTask by
  // calling coin_inserted_isr().
  dispatcher.RunToCompletion();

  return 0;
}
