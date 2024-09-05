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
include_guard(GLOBAL)

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

set(pw_unit_test_BACKEND ="pw_unit_test.light"
  CACHE STRING
  "The unit test framework implementation. Defaults to pw_unit_test.light, which implements a subset of GoogleTest safe to run on device.")

set(pw_unit_test_MAIN "pw_unit_test.simple_printing_main"
  CACHE STRING
  "Implementation of a main function for ``pw_test`` unit test binaries. Must be set to an appropriate target for the pw_unit_test backend.")
