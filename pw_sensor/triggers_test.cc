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

#define TEST_TRIGGER(_name, _expected_name_str)             \
  TEST(SensorTriggers, _name) {                             \
    constexpr uint32_t kExpected##_name##TriggerType =      \
        PW_TOKENIZE_STRING_DOMAIN("PW_SENSOR_TRIGGER_TYPE", \
                                  _expected_name_str);      \
    static_assert(triggers::k##_name::kTriggerType ==       \
                  kExpected##_name##TriggerType);           \
  }

TEST_TRIGGER(DataReady, "data ready")

}  // namespace
}  // namespace pw::sensor
