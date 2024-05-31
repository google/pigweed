// Copyright 2024 The Pigweed Authors
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

#include "pw_digital_io/polarity.h"
#include "pw_digital_io_linux/digital_io.h"
#include "pw_log/log.h"
#include "pw_status/try.h"

using pw::digital_io::LinuxDigitalIoChip;
using pw::digital_io::LinuxInputConfig;
using pw::digital_io::Polarity;
using pw::digital_io::State;

pw::Status InputExample() {
  // Open handle to chip.
  PW_TRY_ASSIGN(auto chip, LinuxDigitalIoChip::Open("/dev/gpiochip0"));

  // Configure input line.
  LinuxInputConfig config(
      /* index= */ 5,
      /* polarity= */ Polarity::kActiveHigh);
  PW_TRY_ASSIGN(auto input, chip.GetInputLine(config));
  PW_TRY(input.Enable());

  // Get the input pin state.
  PW_TRY_ASSIGN(State pin_state, input.GetState());
  PW_LOG_DEBUG("Pin state: %s",
               pin_state == State::kActive ? "active" : "inactive");

  return pw::OkStatus();
}
