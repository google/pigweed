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

#include "gtest/gtest.h"
#include "pw_polyfill/language_feature_macros.h"
#include "pw_polyfill/standard.h"

namespace pw {
namespace polyfill {
namespace {

PW_INLINE_VARIABLE constexpr int foo = 42;

static_assert(foo == 42, "Error!");

static_assert(PW_CXX_STANDARD_IS_SUPPORTED(98), "C++98 must be supported");
static_assert(PW_CXX_STANDARD_IS_SUPPORTED(11), "C++11 must be supported");
static_assert(PW_CXX_STANDARD_IS_SUPPORTED(14), "C++14 must be supported");

#if __cplusplus >= 201703L
static_assert(PW_CXX_STANDARD_IS_SUPPORTED(17), "C++17 must be not supported");
#else
static_assert(!PW_CXX_STANDARD_IS_SUPPORTED(17), "C++17 must be supported");
#endif  // __cplusplus >= 201703L

#if __cplusplus >= 202002L
static_assert(PW_CXX_STANDARD_IS_SUPPORTED(20), "C++20 must be supported");
#else
static_assert(!PW_CXX_STANDARD_IS_SUPPORTED(20), "C++20 must not be supported");
#endif  // __cplusplus >= 202002L

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
