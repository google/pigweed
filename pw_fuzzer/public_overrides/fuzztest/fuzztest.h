// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

/// @file fuzztest.h
/// This file defines no-op versions of the `FUZZ_TEST` and `FUZZ_TEST_F` macros
/// that can be used when FuzzTest is not available.
#pragma once

namespace fuzztest {

struct UnsupportedFuzzTest {
  template <typename Function>
  UnsupportedFuzzTest& IgnoreFunction(Function) {
    return *this;
  }

  template <typename... Args>
  UnsupportedFuzzTest& WithDomains_FUZZTEST_NOT_PRESENT() {
    return *this;
  }

  template <typename... Args>
  UnsupportedFuzzTest& WithSeeds_FUZZTEST_NOT_PRESENT() {
    return *this;
  }
};

}  // namespace fuzztest

#define WithDomains(...) WithDomains_FUZZTEST_NOT_PRESENT()
#define WithSeeds(...) WithSeeds_FUZZTEST_NOT_PRESENT()

#define FUZZ_TEST(test_suite_name, test_name)                             \
  TEST(test_suite_name, DISABLED_##test_name) {}                          \
  fuzztest::UnsupportedFuzzTest                                           \
      _pw_fuzzer_##test_suite_name##_##test_name##_FUZZTEST_NOT_PRESENT = \
          fuzztest::UnsupportedFuzzTest().IgnoreFunction(test_name)

#define FUZZ_TEST_F(test_fixture, test_name)                           \
  TEST_F(test_fixture, DISABLED_##test_name) {}                        \
  fuzztest::UnsupportedFuzzTest                                        \
      _pw_fuzzer_##test_fixture##_##test_name##_FUZZTEST_NOT_PRESENT = \
          fuzztest::UnsupportedFuzzTest().IgnoreFunction(test_name)
