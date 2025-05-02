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

#include "pw_digital_io/digital_io.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "pw_digital_io_zephyr/digital_io.h"
#include "pw_unit_test/framework.h"

namespace pw::digital_io {
namespace {

constexpr const struct gpio_dt_spec kZephyrGpio = {
    .port = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
    .pin = 0,
    .dt_flags = 0,
};

TEST(DigitalIoTest, ReadDigitalIn) {
  ZephyrDigitalIn gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  auto state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kActive);

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kInactive);
}

TEST(DigitalIoTest, ReadDigitalInInterrupt) {
  ZephyrDigitalInInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  auto state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kActive);

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kInactive);
}

TEST(DigitalIoTest, TriggerDigitalInInterrupt) {
  int trigger_count = 0;
  ZephyrDigitalInInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetInterruptHandler(
                InterruptTrigger::kBothEdges,
                [&trigger_count](State sampled_state) {
                  if (trigger_count == 0 && sampled_state == State::kActive) {
                    trigger_count++;
                  } else if (trigger_count == 1 &&
                             sampled_state == State::kInactive) {
                    trigger_count++;
                  }
                }),
            pw::OkStatus());

  ASSERT_EQ(gpio.EnableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 2);

  trigger_count = 0;
  ASSERT_EQ(gpio.DisableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 0);
}

TEST(DigitalIoTest, ReadDigitalInOut) {
  ZephyrDigitalInOut gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  auto state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kActive);

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kInactive);
}

TEST(DigitalIoTest, WriteDigitalInOut) {
  ZephyrDigitalInOut gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetStateActive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 1);

  ASSERT_EQ(gpio.SetStateInactive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 0);
}

TEST(DigitalIoTest, ReadDigitalInOutInterrupt) {
  ZephyrDigitalInOutInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  auto state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kActive);

  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  state = gpio.GetState();
  ASSERT_EQ(state.status(), pw::OkStatus());
  ASSERT_EQ(state.value(), State::kInactive);
}

TEST(DigitalIoTest, WriteDigitalInOutInterrupt) {
  ZephyrDigitalInOutInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetStateActive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 1);

  ASSERT_EQ(gpio.SetStateInactive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 0);
}

TEST(DigitalIoTest, TriggerDigitalInOutInterrupt) {
  int trigger_count = 0;
  ZephyrDigitalInOutInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetInterruptHandler(
                InterruptTrigger::kBothEdges,
                [&trigger_count](State sampled_state) {
                  if (trigger_count == 0 && sampled_state == State::kActive) {
                    trigger_count++;
                  } else if (trigger_count == 1 &&
                             sampled_state == State::kInactive) {
                    trigger_count++;
                  }
                }),
            pw::OkStatus());

  ASSERT_EQ(gpio.EnableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 2);

  trigger_count = 0;
  ASSERT_EQ(gpio.DisableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 0);
}

TEST(DigitalIoTest, TriggerDigitalInterrupt) {
  int trigger_count = 0;
  ZephyrDigitalInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetInterruptHandler(
                InterruptTrigger::kBothEdges,
                [&trigger_count](State sampled_state) {
                  if (trigger_count == 0 && sampled_state == State::kActive) {
                    trigger_count++;
                  } else if (trigger_count == 1 &&
                             sampled_state == State::kInactive) {
                    trigger_count++;
                  }
                }),
            pw::OkStatus());

  ASSERT_EQ(gpio.EnableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 2);

  trigger_count = 0;
  ASSERT_EQ(gpio.DisableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 0);
}

TEST(DigitalIoTest, WriteDigitalOut) {
  ZephyrDigitalOut gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetStateActive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 1);

  ASSERT_EQ(gpio.SetStateInactive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 0);
}

TEST(DigitalIoTest, WriteDigitalOutInterrupt) {
  ZephyrDigitalOutInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetStateActive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 1);

  ASSERT_EQ(gpio.SetStateInactive(), pw::OkStatus());
  ASSERT_EQ(gpio_emul_output_get(kZephyrGpio.port, kZephyrGpio.pin), 0);
}

TEST(DigitalIoTest, TriggerDigitalOutInterrupt) {
  int trigger_count = 0;
  ZephyrDigitalOutInterrupt gpio(kZephyrGpio);
  ASSERT_EQ(gpio.Enable(), pw::OkStatus());

  ASSERT_EQ(gpio.SetInterruptHandler(
                InterruptTrigger::kBothEdges,
                [&trigger_count](State sampled_state) {
                  if (trigger_count == 0 && sampled_state == State::kActive) {
                    trigger_count++;
                  } else if (trigger_count == 1 &&
                             sampled_state == State::kInactive) {
                    trigger_count++;
                  }
                }),
            pw::OkStatus());

  ASSERT_EQ(gpio.EnableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 2);

  trigger_count = 0;
  ASSERT_EQ(gpio.DisableInterruptHandler(), pw::OkStatus());
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 1);
  gpio_emul_input_set(kZephyrGpio.port, kZephyrGpio.pin, 0);
  ASSERT_EQ(trigger_count, 0);
}
}  // namespace
}  // namespace pw::digital_io
