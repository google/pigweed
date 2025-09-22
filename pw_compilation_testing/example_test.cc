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

#include "pw_assert/assert.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_polyfill/standard.h"
#include "pw_unit_test/framework.h"

namespace {

template <int kValue>
struct MyStruct {
  static_assert(kValue % 2 == 0, "wrong number!");

  constexpr MyStruct() = default;

  MyStruct(int) {}  // non-constexpr constructor

  constexpr int MultiplyOdd(int runtime_value) const {
    PW_ASSERT(runtime_value % 2 == 1);
    return kValue * runtime_value;
  }
};

[[maybe_unused]] MyStruct<16> this_one_works;

// NC tests cannot be compiled, so they are created in preprocessor #if or
// #elif blocks. These NC tests check that a static_assert statement fails if
// the code is compiled.
#if PW_NC_TEST(NegativeOddNumber)
PW_NC_EXPECT("wrong number!");
[[maybe_unused]] MyStruct<-1> illegal;
#elif PW_NC_TEST(PositiveOddNumber)
PW_NC_EXPECT("wrong number!");
[[maybe_unused]] MyStruct<5> this_is_illegal;
#endif  // PW_NC_TEST

struct Foo {
  // Negative compilation tests can go anywhere in a source file.
#if PW_NC_TEST(IllegalValueAsClassMember)
  PW_NC_EXPECT("wrong number!");
  MyStruct<13> also_illegal;
#endif  // PW_NC_TEST
};

// Negative compilation tests can be integrated with standard unit tests.
TEST(MyStruct, MultiplyOdd) {
  constexpr MyStruct<6> six;
  EXPECT_EQ(six.MultiplyOdd(3), 18);

// This NC test checks that a specific PW_ASSERT() fails when expected.
// This only works in an NC test if the PW_ASSERT() fails while the compiler
// is executing constexpr code. The test code is used in a constexpr
// statement to force compile-time evaluation.
#if PW_NC_TEST(MyStruct_MultiplyOdd_AssertsOnOddNumber)
  PW_NC_EXPECT("PW_ASSERT\(runtime_value % 2 == 1\);");
  [[maybe_unused]] constexpr auto fail = [&six] {
    return six.MultiplyOdd(4);  // Even number, PW_ASSERT should fail.
  }();
#endif  // PW_NC_TEST
}

// PW_NC_TESTs can be skipped by commenting them out or using `#if 0`.
#if PW_CXX_STANDARD_IS_SUPPORTED(20)
#if PW_NC_TEST(DoesNotCompile)
PW_NC_EXPECT("PW_ASSERT_failed_in_constant_expression_");
[[maybe_unused]] constinit MyStruct<12> should_fail = [] {
  MyStruct<12> example;
  example.MultiplyOdd(2);
  return example;
}();
#endif  // PW_NC_TEST
#endif  // PW_CXX_STANDARD_IS_SUPPORTED(20)

}  // namespace
