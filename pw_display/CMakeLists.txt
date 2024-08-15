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

pw_add_library(pw_display.color INTERFACE
  HEADERS
    public/pw_display/color.h
    public/pw_display/colors_endesga64.h
    public/pw_display/colors_pico8.h
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_display.color_test
  SOURCES
    color_test.cc
  PRIVATE_DEPS
    pw_display.color
  GROUPS
    modules
    pw_display
)
