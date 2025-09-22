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

#include "pw_compilation_testing/negative_compilation.h"
#include "pw_unit_test/framework.h"

namespace {

#if PW_NC_TEST(TestOne)
PW_NC_EXPECT("major failure");
#error this is a major failure
#elif PW_NC_TEST(TestTwo)
PW_NC_EXPECT("hello world");
static_assert(false, "hello world");
#endif  // PW_NC_TEST

TEST(NcTest, WithinUnitTest) {
  int foo = 3;
  EXPECT_GT(foo, 0);

#if PW_NC_TEST(NcTestWithinUnitTest)
  PW_NC_EXPECT("\bfoo\b");  // Make sure the error includes the "foo" variable
  PW_NC_EXPECT("\bint\b");  // and references the int type

  foo = "hello world!";
#endif  // PW_NC_TEST
}

#if PW_NC_TEST(ClangAndGccVariants)
PW_NC_EXPECT_CLANG("You're using Clang!");
PW_NC_EXPECT_GCC("You're using GCC!");

#ifdef __clang__
#error "You're using Clang!"
#else
#error "You're using GCC!"
#endif  // __clang__

#endif  // PW_NC_TEST

#ifndef THIS_SHOULD_BE_DEFINED
#error "defines must be forwarded"
#endif  // THIS_SHOULD_BE_DEFINED

static_assert(THIS_SHOULD_BE_42 == 42, "copts must be forwarded");

}  // namespace
