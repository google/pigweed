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
#include "pw_digital_io/digital_io.h"

#include "pw_digital_io_rp2040/digital_io.h"
#include "pw_result/result.h"
#include "pw_unit_test/framework.h"

using pw::digital_io::Rp2040DigitalIn;
using pw::digital_io::Rp2040DigitalInOut;

namespace pw::digital_io {
namespace {

constexpr Rp2040Config output_pin_config{
    .pin = 15,
    .polarity = Polarity::kActiveLow,
};
constexpr Rp2040Config input_pin_config{
    .pin = 16,
    .polarity = Polarity::kActiveHigh,
};
Rp2040DigitalInOut output_pin(output_pin_config);
Rp2040DigitalIn input_pin(input_pin_config);

TEST(DigitalIoTest, PhysicalToLogical) {
  ASSERT_EQ(State::kActive, output_pin_config.PhysicalToLogical(false));
  ASSERT_EQ(State::kInactive, output_pin_config.PhysicalToLogical(true));
  ASSERT_EQ(State::kActive, input_pin_config.PhysicalToLogical(true));
  ASSERT_EQ(State::kInactive, input_pin_config.PhysicalToLogical(false));
}

TEST(DigitalIoTest, LogicalToPhysical) {
  ASSERT_EQ(false, output_pin_config.LogicalToPhysical(State::kActive));
  ASSERT_EQ(true, output_pin_config.LogicalToPhysical(State::kInactive));
  ASSERT_EQ(true, input_pin_config.LogicalToPhysical(State::kActive));
  ASSERT_EQ(false, input_pin_config.LogicalToPhysical(State::kInactive));
}

TEST(DigitalIoTest, Init) {
  // Simple test only meant to ensure module is compiled.
  output_pin.Enable();
  output_pin.SetState(pw::digital_io::State::kActive);

  input_pin.Enable();
  Result<State> state_result = input_pin.GetState();
  ASSERT_EQ(OkStatus(), state_result.status());
  ASSERT_EQ(State::kInactive, state_result.value());
}

}  // namespace
}  // namespace pw::digital_io
