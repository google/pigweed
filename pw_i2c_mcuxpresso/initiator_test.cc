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
#include "pw_i2c_mcuxpresso/initiator.h"

#include <gtest/gtest.h>

namespace pw::i2c {
namespace {

constexpr uint32_t kI2CBaudRate = 100000;

TEST(InitiatorTest, Init) {
  // Simple test only meant to ensure module is compiled.
  const auto clock_freq = CLOCK_GetFlexcommClkFreq(11);
  McuxpressoInitiator initiator(I2C11, kI2CBaudRate, clock_freq);
}

}  // namespace
}  // namespace pw::i2c
