# Copyright 2023 The Pigweed Authors
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

################################################################################
# FuzzTest support
#
# Create FuzzTest-style fuzzers by adding a dep on pw_fuzzer.fuzztest

# If a project configure `pw_unit_test_GOOGLETEST_BACKEND` to use FuzzTest, this
# target pulls in the "real" FuzzTest. Otherwise, it uses a "stub" which
# converts invocations of `FUZZ_TEST` to disabled empty unit tests.
if(pw_unit_test_GOOGLETEST_BACKEND STREQUAL "pw_third_party.fuzztest")
  pw_add_library(pw_fuzzer.fuzztest INTERFACE
    PUBLIC_DEPS
      pw_third_party.fuzztest
  )
else()
  pw_add_library(pw_fuzzer.fuzztest INTERFACE
    HEADERS
      public_overrides/fuzztest/fuzztest.h
    PUBLIC_INCLUDES
      public_overrides
  )
endif()

add_subdirectory("examples/fuzztest")
