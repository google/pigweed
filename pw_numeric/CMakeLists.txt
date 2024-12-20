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

pw_add_library(pw_numeric.checked_arithmetic INTERFACE
  HEADERS
    public/pw_numeric/checked_arithmetic.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_preprocessor
)

pw_add_test(pw_numeric.checked_arithmetic_test
  SOURCES
    checked_arithmetic_test.cc
  PRIVATE_DEPS
    pw_numeric.checked_arithmetic
  GROUPS
    modules
    pw_numeric
)
pw_add_library(pw_numeric.integer_division INTERFACE
  HEADERS
    public/pw_numeric/integer_division.h
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_numeric.integer_division_test
  SOURCES
    integer_division_test.cc
  PRIVATE_DEPS
    pw_numeric.integer_division
  GROUPS
    modules
    pw_numeric
)
