// Copyright 2020 The Pigweed Authors
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

#include "pw_polyfill/language_feature_macros.h"
#include "pw_polyfill/standard.h"
#include "pw_unit_test/framework.h"

namespace pw {
namespace polyfill {
namespace {

TEST(CppStandardMacro, MacroIsTrueUpToCpp14SincePigweedRequiresCpp14) {
  static_assert(PW_CXX_STANDARD_IS_SUPPORTED(98),
                "Macro must be true since Pigweed requires support for C++98");
  static_assert(PW_CXX_STANDARD_IS_SUPPORTED(11),
                "Macro must be true since Pigweed requires support for C++11");
  static_assert(PW_CXX_STANDARD_IS_SUPPORTED(14),
                "Macro must be true since Pigweed requires support for C++14");
}

TEST(CppStandardMacro, MacroIsTrueIfSpecifiedStandardIsSupported) {
  static_assert(PW_CXX_STANDARD_IS_SUPPORTED(17) == (__cplusplus >= 201703L),
                "PW_CXX_STANDARD_IS_SUPPORTED() should match __cplusplus");
  static_assert(PW_CXX_STANDARD_IS_SUPPORTED(20) == (__cplusplus >= 202002L),
                "PW_CXX_STANDARD_IS_SUPPORTED() should match __cplusplus");
  static_assert(PW_CXX_STANDARD_IS_SUPPORTED(23) == (__cplusplus >= 202302L),
                "PW_CXX_STANDARD_IS_SUPPORTED() should match __cplusplus");
}

TEST(CStandardMacro, MacroIsFalseSinceThisIsACppFile) {
  static_assert(!PW_C_STANDARD_IS_SUPPORTED(89),
                "PW_C_STANDARD_IS_SUPPORTED() must always be false in C++");
  static_assert(!PW_C_STANDARD_IS_SUPPORTED(99),
                "PW_C_STANDARD_IS_SUPPORTED() must always be false in C++");
  static_assert(!PW_C_STANDARD_IS_SUPPORTED(11),
                "PW_C_STANDARD_IS_SUPPORTED() must always be false in C++");
  static_assert(!PW_C_STANDARD_IS_SUPPORTED(17),
                "PW_C_STANDARD_IS_SUPPORTED() must always be false in C++");
  static_assert(!PW_C_STANDARD_IS_SUPPORTED(23),
                "PW_C_STANDARD_IS_SUPPORTED() must always be false in C++");
}

// Check that consteval is at least equivalent to constexpr.
PW_CONSTEVAL int ConstevalFunction() { return 123; }
static_assert(ConstevalFunction() == 123,
              "Function should work in static_assert");

PW_CONSTINIT bool mutable_value = true;

TEST(Constinit, ValueIsMutable) {
  ASSERT_TRUE(mutable_value);
  mutable_value = false;
  ASSERT_FALSE(mutable_value);
  mutable_value = true;
}

}  // namespace
}  // namespace polyfill
}  // namespace pw
