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
include($ENV{PW_ROOT}/pw_unit_test/test.cmake)

pw_add_library(pw_fuzzer.examples.fuzztest.metrics_lib STATIC
  HEADERS
    metrics.h
  PUBLIC_INCLUDES
    .
  PUBLIC_DEPS
    pw_bytes
    pw_assert
  SOURCES
    metrics.cc
)

pw_add_test(pw_fuzzer.examples.fuzztest.metrics_unittest
  SOURCES
    metrics_unittest.cc
  PRIVATE_DEPS
    pw_fuzzer.examples.fuzztest.metrics_lib
  GROUPS
    modules
    pw_fuzzer
)

# DOCSTAG: [pwfuzzer_examples_fuzztest-cmake]
pw_add_test(pw_fuzzer.examples.fuzztest.metrics_fuzztest
  SOURCES
    metrics_fuzztest.cc
  PRIVATE_DEPS
    pw_fuzzer.fuzztest  # <- Added!
    pw_fuzzer.examples.fuzztest.metrics_lib
  GROUPS
    modules
    pw_fuzzer
)
# DOCSTAG: [pwfuzzer_examples_fuzztest-cmake]
