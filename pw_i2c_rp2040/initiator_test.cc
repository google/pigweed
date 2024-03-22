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
#include "pw_i2c_rp2040/initiator.h"

#include <gtest/gtest.h>

#include "hardware/i2c.h"

namespace pw::i2c {
namespace {

constexpr pw::i2c::Rp2040Initiator::Config ki2cConfig{
    .clock_frequency = 400'000,
    .sda_pin = 8,
    .scl_pin = 9,
};

TEST(InitiatorTest, Init) {
  // Simple test only meant to ensure module is compiled.
  pw::i2c::Rp2040Initiator i2c_bus(ki2cConfig, i2c0);
  i2c_bus.Enable();
  i2c_bus.Disable();
}

}  // namespace
}  // namespace pw::i2c
