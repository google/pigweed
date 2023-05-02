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
include_guard(GLOBAL)

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_unit_test/test.cmake)

# pw_add_fuzz_test: Declares a single fuzz test suite with Pigweed naming rules
#                   and compiler warning options.
#
# By default, this produces a unit test suite identical to what would be
# produced by `pw_add_test`. When FuzzTest and its dependencies are available,
# and the build is configured to instrument the code for fuzzing, the test suite
# will also include fuzz tests that can be run for an arbitrary amount of time.
#
# See also: https://github.com/google/fuzztest
#
# Arguments are identical to those for `pw_add_test`.
#
# TODO(b/280457542): Add CMake support for FuzzTest.
function(pw_add_fuzz_test NAME)
  pw_parse_arguments(
    NUM_POSITIONAL_ARGS
      1
    MULTI_VALUE_ARGS
      SOURCES HEADERS PRIVATE_DEPS PRIVATE_INCLUDES
      PRIVATE_DEFINES PRIVATE_COMPILE_OPTIONS
      PRIVATE_LINK_OPTIONS GROUPS
  )

  pw_add_test("${NAME}"
    SOURCES
      ${arg_SOURCES}
    HEADERS
      ${arg_HEADERS}
    PRIVATE_DEPS
      pw_fuzzer.fuzz_test_stub
      ${arg_PRIVATE_DEPS}
    PRIVATE_INCLUDES
      ${arg_PRIVATE_INCLUDES}
    PRIVATE_DEFINES
      ${arg_PRIVATE_DEFINES}
    PRIVATE_COMPILE_OPTIONS
      ${arg_PRIVATE_COMPILE_OPTIONS}
    PRIVATE_LINK_OPTIONS
      ${arg_PRIVATE_LINK_OPTIONS}
    GROUPS
      ${arg_GROUPS}
  )
endfunction()
