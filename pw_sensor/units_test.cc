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

#include "pw_sensor/generated/sensor_constants.h"
#include "pw_tokenizer/tokenize.h"
#include "pw_unit_test/framework.h"

namespace pw::sensor {
namespace {

#define TEST_UNIT(_name, _expected_name_str, _expected_unit_str)              \
  TEST(SensorUnits, _name) {                                                  \
    constexpr auto kExpected##_name##UnitName = PW_TOKENIZE_STRING_MASK(      \
        "PW_SENSOR_UNITS_TYPE", 0xFFFF, _expected_name_str);                  \
    constexpr auto kExpected##_name##UnitSymbol = PW_TOKENIZE_STRING_MASK(    \
        "PW_SENSOR_UNITS_TYPE", 0xFFFF, _expected_unit_str);                  \
    static_assert(units::k##_name::kUnitName == kExpected##_name##UnitName);  \
    static_assert(units::k##_name::kUnitSymbol ==                             \
                  kExpected##_name##UnitSymbol);                              \
    static_assert(                                                            \
        units::k##_name::kUnitType ==                                         \
        ((kExpected##_name##UnitName << 16) | kExpected##_name##UnitSymbol)); \
  }

TEST_UNIT(Acceleration, "acceleration", "m/s^2")
TEST_UNIT(Frequency, "frequency", "Hz")
TEST_UNIT(MagneticField, "magnetic field", "Gs")
TEST_UNIT(RotationalVelocity, "rotational velocity", "rad/s")
TEST_UNIT(Temperature, "temperature", "C")
TEST_UNIT(TimeCycles, "time cycles", "cycles")

}  // namespace
}  // namespace pw::sensor
