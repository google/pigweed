// Copyright 2023 The Pigweed Authors
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

#include "pw_digital_io_rp2040/digital_io.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pw_status/status.h"

namespace pw::digital_io {

Rp2040DigitalIn::Rp2040DigitalIn(uint32_t pin) : pin_(pin) {}

Status Rp2040DigitalIn::DoEnable(bool enable) {
  if (!enable) {
    gpio_deinit(pin_);
    return OkStatus();
  }

  gpio_init(pin_);
  gpio_set_dir(pin_, GPIO_IN);
  return OkStatus();
}

Result<State> Rp2040DigitalIn::DoGetState() {
  const pw::Result<State> result(State(gpio_get(pin_)));
  return result;
}

Rp2040DigitalInOut::Rp2040DigitalInOut(uint32_t pin) : pin_(pin) {}

Status Rp2040DigitalInOut::DoEnable(bool enable) {
  if (!enable) {
    gpio_deinit(pin_);
    return OkStatus();
  }

  gpio_init(pin_);
  gpio_set_dir(pin_, GPIO_OUT);
  return OkStatus();
}

Status Rp2040DigitalInOut::DoSetState(State level) {
  gpio_put(pin_, level == State::kActive);
  return OkStatus();
}

Result<State> Rp2040DigitalInOut::DoGetState() {
  const pw::Result<State> result(State(gpio_get(pin_)));
  return result;
}

}  // namespace pw::digital_io
