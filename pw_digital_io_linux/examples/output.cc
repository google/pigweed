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
#include "pw_status/try.h"

using pw::digital_io::LinuxDigitalIoChip;
using pw::digital_io::LinuxOutputConfig;
using pw::digital_io::Polarity;
using pw::digital_io::State;

pw::Status OutputExample() {
  // Open handle to chip.
  PW_TRY_ASSIGN(auto chip, LinuxDigitalIoChip::Open("/dev/gpiochip0"));

  // Configure output line.
  // Set the polarity to active-low and default state to active.
  LinuxOutputConfig config(
      /* index= */ 4,
      /* polarity= */ Polarity::kActiveLow,
      /* default_state= */ State::kActive);
  PW_TRY_ASSIGN(auto output, chip.GetOutputLine(config));

  // Enable the output pin. This pulls the pin to ground since the
  // polarity is kActiveLow and the default_state is kActive.
  PW_TRY(output.Enable());

  // Set the output pin to inactive.
  // This pulls pin to Vdd since the polarity is kActiveLow.
  PW_TRY(output.SetState(State::kInactive));

  return pw::OkStatus();
}
