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
#pragma once

#if !defined(PW_BUILD_EXPECTED_SOURCE_PATH) || \
    !defined(PW_BUILD_EXPECTED_HEADER_PATH)
#error "Expected source and header path macros must be defined!"
#endif

namespace pw::build::test {

constexpr bool StringsAreEqual(const char* left, const char* right) {
  while (*left == *right && *left != '\0') {
    left += 1;
    right += 1;
  }

  return *left == '\0' && *right == '\0';
}

static_assert(StringsAreEqual(PW_BUILD_EXPECTED_HEADER_PATH, __FILE__),
              "The __FILE__ macro should be " PW_BUILD_EXPECTED_HEADER_PATH
              ", but it is " __FILE__);

}  // namespace pw::build::test
