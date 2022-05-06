# Copyright 2020 The Pigweed Authors
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

pw_add_module_library(pw_gpio
  HEADERS
    public/pw_gpio/gpio.h
    public/pw_gpio/internal/conversions.h
  PUBLIC_INCLUDES
    public
  SOURCES
    gpio.cc
  PUBLIC_DEPS
    pw_assert
    pw_function
    pw_result
    pw_status
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_GPIO)
  zephyr_link_libraries(pw_gpio)
endif()

pw_add_test(pw_gpio.stream_test
  SOURCES
    gpio_test.cc
  DEPS
    pw_gpio
  GROUPS
    modules
    pw_gpio
)
