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

#include "pw_async2/try.h"
#include "pw_log/log.h"

namespace codelab {

pw::async2::Poll<int> Keypad::Pend(pw::async2::Context& cx) {
  return key_pressed_;
}

void Keypad::Press(int key) {}

pw::async2::Poll<> VendingMachineTask::DoPend(pw::async2::Context& cx) {
  if (!displayed_welcome_message_) {
    PW_LOG_INFO("Welcome to the Pigweed Vending Machine!");
    PW_LOG_INFO("Please insert a coin.");
    displayed_welcome_message_ = true;
  }

  if (coins_inserted_ == 0) {
    PW_TRY_READY_ASSIGN(unsigned coins, coin_slot_.Pend(cx));
    PW_LOG_INFO("Received %u coin%s.", coins, coins > 1 ? "s" : "");
    PW_LOG_INFO("Please press a keypad key.");
    coins_inserted_ += coins;
  }

  PW_TRY_READY_ASSIGN(int key, keypad_.Pend(cx));
  PW_LOG_INFO("Keypad %d was pressed. Dispensing an item.", key);

  return pw::async2::Ready();
}

}  // namespace codelab
