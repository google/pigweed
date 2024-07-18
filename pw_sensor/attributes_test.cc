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

#define TEST_ATTRIBUTE(_name, _expected_name_str)             \
  TEST(SensorAttributes, _name) {                             \
    constexpr auto kExpected##_name##AttributeType =          \
        PW_TOKENIZE_STRING_DOMAIN("PW_SENSOR_ATTRIBUTE_TYPE", \
                                  _expected_name_str);        \
    static_assert(attributes::k##_name::kAttributeType ==     \
                  kExpected##_name##AttributeType);           \
  }

TEST_ATTRIBUTE(SampleRate, "sample rate")
TEST_ATTRIBUTE(Range, "range")
TEST_ATTRIBUTE(BatchDuration, "batch duration")

}  // namespace

}  // namespace pw::sensor
