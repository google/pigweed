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

pw_add_module_library(pw_unit_test
  SOURCES
    framework.cc
  PUBLIC_DEPS
    pw_polyfill
    pw_preprocessor
    pw_string
)

# pw_unit_test overrides the gtest/gtest.h header.
target_include_directories(pw_unit_test PUBLIC public_overrides)

pw_add_module_library(pw_unit_test.main
  SOURCES
    simple_printing_main.cc
    simple_printing_event_handler.cc
  PUBLIC_DEPS
    pw_unit_test
  PRIVATE_DEPS
    pw_preprocessor
    pw_string
    pw_sys_io
)
