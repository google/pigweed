// Copyright 2025 The Pigweed Authors
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

#include "pw_unit_test/framework.h"

// DOCSTAG[pw_assert_log-mod-example]
#include <functional>

#include "pw_assert/assert.h"

namespace examples {

void AssertValueIsOdd(int value) {
  // This will not compile with pw_assert_log due to use of the % character:
  // PW_ASSERT(value % 2 != 0);

  // Instead, store the result in a variable.
  const int mod_2 = value % 2;
  PW_ASSERT(mod_2 != 0);

  // Or, perform the % operation in a function, such as std::modulus.
  PW_ASSERT(std::modulus{}(value, 2) != 0);
}

}  // namespace examples
// DOCSTAG[pw_assert_log-mod-example]

namespace {

TEST(AssertExamples, AssertValueIsOdd) {
  examples::AssertValueIsOdd(1);
  examples::AssertValueIsOdd(3);
}

}  // namespace
