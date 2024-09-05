# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_sensor/sensor.cmake)

pw_add_library(pw_sensor.types INTERFACE
  HEADERS
    public/pw_sensor/types.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_tokenizer
)

pw_sensor_library(pw_sensor.test_constants
  OUT_HEADER
    pw_sensor/generated/sensor_constants.h
  INPUTS
    $ENV{PW_ROOT}/pw_sensor/attributes.yaml
    $ENV{PW_ROOT}/pw_sensor/channels.yaml
    $ENV{PW_ROOT}/pw_sensor/triggers.yaml
    $ENV{PW_ROOT}/pw_sensor/units.yaml
  GENERATOR_INCLUDES
    $ENV{PW_ROOT}
  SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/sensor.yaml
  PUBLIC_DEPS
    pw_sensor.types
    pw_containers
    pw_tokenizer
)

pw_add_library(pw_sensor INTERFACE
  HEADERS
    public/pw_sensor/types.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_tokenizer
)

# This test does not run on CMake due to missing pw_sensor_library function.
pw_add_test(pw_sensor.constants_test
  SOURCES
    attributes_test.cc
    channels_test.cc
    triggers_test.cc
    units_test.cc
  PRIVATE_DEPS
    pw_sensor
    pw_sensor.test_constants
  GROUPS
    modules
    pw_sensor
)
