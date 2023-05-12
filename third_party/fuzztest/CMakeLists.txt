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

if(NOT dir_pw_third_party_fuzztest)
  pw_add_error_target(pw_third_party.fuzztest
    MESSAGE
      "Attempted to build the pw_third_party.fuzztest without configuring it "
      "via dir_pw_third_party_fuzztest. "
      "See https://pigweed.dev/third_party/fuzztest."
  )
  pw_add_error_target(pw_third_party.fuzztest_gtest_main
    MESSAGE
      "Attempted to build the pw_third_party.fuzztest_gtest_main without "
      "configuring it via dir_pw_third_party_fuzztest. "
      "See https://pigweed.dev/third_party/fuzztest."
  )
  return()
else()
  add_subdirectory("${dir_pw_third_party_fuzztest}" third_party/fuzztest)
endif()

# See also //pw_fuzzer:fuzztest.
add_library(pw_third_party.fuzztest INTERFACE)
target_link_libraries(pw_third_party.fuzztest
  INTERFACE
    fuzztest_fuzztest
    pw_third_party.googletest
)
target_include_directories(pw_third_party.fuzztest
  INTERFACE
    "${dir_pw_third_party_fuzztest}"
)

# See also //target/host/pw_add_test_executable.cmake.
add_library(pw_third_party.fuzztest_gtest_main INTERFACE)
target_link_libraries(pw_third_party.fuzztest_gtest_main
  INTERFACE
    fuzztest_gtest_main
)
