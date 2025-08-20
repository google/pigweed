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

// Interrupt handler function invoked when the user inserts a coin into the
// vending machine.
void coin_inserted_isr();

// Interrupt handler function invoked when the user presses a key on the
// machine's keypad. Receives the value of the pressed key (0-9).
void key_press_isr(int key);

namespace codelab {

// Initializes the simulated hardware, allowing for interactive input via stdin
// in a background thread.
void HardwareInit();

}  // namespace codelab
