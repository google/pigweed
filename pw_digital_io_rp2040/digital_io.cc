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
#include "pw_digital_io/digital_io.h"
#include "pw_status/status.h"

namespace pw::digital_io {

Rp2040DigitalIn::Rp2040DigitalIn(Rp2040Config config) : config_(config) {}

Status Rp2040DigitalIn::DoEnable(bool enable) {
  if (!enable) {
    gpio_deinit(config_.pin);
    return OkStatus();
  }

  gpio_init(config_.pin);
  gpio_set_dir(config_.pin, GPIO_IN);
  return OkStatus();
}

Result<State> Rp2040DigitalIn::DoGetState() {
  if (gpio_get_function(config_.pin) != GPIO_FUNC_SIO ||
      gpio_get_dir(config_.pin) != GPIO_IN) {
    return Status::FailedPrecondition();
  }

  const bool pin_value = gpio_get(config_.pin);
  const State state = config_.PhysicalToLogical(pin_value);
  return pw::Result<State>(state);
}

Rp2040DigitalInOut::Rp2040DigitalInOut(Rp2040Config config) : config_(config) {}

Status Rp2040DigitalInOut::DoEnable(bool enable) {
  if (!enable) {
    gpio_deinit(config_.pin);
    return OkStatus();
  }

  gpio_init(config_.pin);
  gpio_set_dir(config_.pin, GPIO_OUT);
  return OkStatus();
}

Status Rp2040DigitalInOut::DoSetState(State level) {
  if (gpio_get_function(config_.pin) != GPIO_FUNC_SIO ||
      gpio_get_dir(config_.pin) != GPIO_OUT) {
    return Status::FailedPrecondition();
  }

  gpio_put(config_.pin, config_.LogicalToPhysical(level));
  return OkStatus();
}

Result<State> Rp2040DigitalInOut::DoGetState() {
  if (gpio_get_function(config_.pin) != GPIO_FUNC_SIO ||
      gpio_get_dir(config_.pin) != GPIO_OUT) {
    return Status::FailedPrecondition();
  }

  const bool pin_value = gpio_get(config_.pin);
  const State state = config_.PhysicalToLogical(pin_value);
  return pw::Result<State>(state);
}

}  // namespace pw::digital_io
