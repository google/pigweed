// Copyright 2019 The Pigweed Authors
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
//
// All of these tests are static asserts. If the test compiles, it has already
// passed. The TEST functions are used for organization only.

#include "pw_preprocessor/macro_arg_count.h"

#include "pw_unit_test/framework.h"

namespace pw {
namespace {

TEST(HasArgs, WithoutArguments) {
  static_assert(PW_HAS_ARGS() == 0);
  static_assert(PW_HAS_ARGS(/**/) == 0);
  static_assert(PW_HAS_ARGS(/* uhm, hi */) == 0);

  // Test how the macro handles whitespace and comments.
  // clang-format off
  static_assert(PW_HAS_ARGS(     ) == 0);  // NOLINT
  static_assert(PW_HAS_ARGS(
      ) == 0);  // NOLINT
  static_assert(PW_HAS_ARGS(
      // wow
      // This is a comment.
      ) == 0);  // NOLINT
  // clang-format on

  static_assert(PW_HAS_NO_ARGS() == 1);
  static_assert(PW_HAS_NO_ARGS(/* hello */) == 1);
  static_assert(PW_HAS_NO_ARGS(
                    // hello
                    /* goodbye */) == 1);
}

TEST(HasArgs, WithArguments) {
  static_assert(PW_HAS_ARGS(()) == 1);
  static_assert(PW_HAS_ARGS(0) == 1);
  static_assert(PW_HAS_ARGS(, ) == 1);  // NOLINT
  static_assert(PW_HAS_ARGS(a, b, c) == 1);
  static_assert(PW_HAS_ARGS(PW_HAS_ARGS) == 1);
  static_assert(PW_HAS_ARGS(PW_HAS_ARGS()) == 1);

  static_assert(PW_HAS_NO_ARGS(0) == 0);
  static_assert(PW_HAS_NO_ARGS(, ) == 0);  // NOLINT
  static_assert(PW_HAS_NO_ARGS(a, b, c) == 0);
  static_assert(PW_HAS_NO_ARGS(PW_HAS_ARGS) == 0);
  static_assert(PW_HAS_NO_ARGS(PW_HAS_ARGS()) == 0);
}

constexpr int TestFunc(int arg, ...) { return arg; }

#define CALL_FUNCTION(arg, ...) TestFunc(arg PW_COMMA_ARGS(__VA_ARGS__))

template <typename T, typename... Args>
constexpr T TemplateArgCount() {
  return sizeof...(Args);
}

#define COUNT_ARGS_TEMPLATE(...) \
  TemplateArgCount<int PW_COMMA_ARGS(__VA_ARGS__)>()

TEST(CommaVarargs, NoArguments) {
  static_assert(TestFunc(0 PW_COMMA_ARGS()) == 0);
  static_assert(TestFunc(1 /* whoa */ PW_COMMA_ARGS(
                    /* this macro */) /* is cool! */) == 1);

  static_assert(TemplateArgCount<int PW_COMMA_ARGS()>() == 0);
  static_assert(TemplateArgCount<int PW_COMMA_ARGS(/* nothing */)>() == 0);

  static_assert(CALL_FUNCTION(2) == 2);
  static_assert(CALL_FUNCTION(3, ) == 3);
  static_assert(CALL_FUNCTION(4, /* nothing */) == 4);

  static_assert(COUNT_ARGS_TEMPLATE() == 0);
  static_assert(COUNT_ARGS_TEMPLATE(/* nothing */) == 0);
}

TEST(CommaVarargs, WithArguments) {
  static_assert(TestFunc(0 PW_COMMA_ARGS(1)) == 0);
  static_assert(TestFunc(1 PW_COMMA_ARGS(1, 2)) == 1);
  static_assert(TestFunc(2 PW_COMMA_ARGS(1, 2, "three")) == 2);

  static_assert(TemplateArgCount<int PW_COMMA_ARGS(bool)>() == 1);
  static_assert(TemplateArgCount<int PW_COMMA_ARGS(char, const char*)>() == 2);
  static_assert(TemplateArgCount<int PW_COMMA_ARGS(int, char, const char*)>() ==
                3);

  static_assert(CALL_FUNCTION(3) == 3);
  static_assert(CALL_FUNCTION(4, ) == 4);
  static_assert(CALL_FUNCTION(5, /* nothing */) == 5);

  static_assert(COUNT_ARGS_TEMPLATE(int) == 1);
  static_assert(COUNT_ARGS_TEMPLATE(int, int) == 2);
  static_assert(COUNT_ARGS_TEMPLATE(int, int, int) == 3);
}

TEST(CountArgs, Zero) {
  static_assert(PW_ARG_COUNT() == 0);
  static_assert(PW_ARG_COUNT(/**/) == 0);
  static_assert(PW_ARG_COUNT(/* uhm, hi */) == 0);

  // clang-format off
  static_assert(PW_ARG_COUNT(     ) == 0);  // NOLINT
  static_assert(PW_ARG_COUNT(
      ) == 0);  // NOLINT
  static_assert(PW_ARG_COUNT(
      // wow
      // This is a comment.
      ) == 0);  // NOLINT
  // clang-format on
}

TEST(CountArgs, Commas) {
  // clang-format off
  static_assert(PW_ARG_COUNT(,) == 2);    // NOLINT
  static_assert(PW_ARG_COUNT(,,) == 3);   // NOLINT
  static_assert(PW_ARG_COUNT(,,,) == 4);  // NOLINT
  // clang-format on
  static_assert(PW_ARG_COUNT(, ) == 2);      // NOLINT
  static_assert(PW_ARG_COUNT(, , ) == 3);    // NOLINT
  static_assert(PW_ARG_COUNT(, , , ) == 4);  // NOLINT
}

TEST(CountArgs, Parentheses) {
  static_assert(PW_ARG_COUNT(()) == 1);
  static_assert(PW_ARG_COUNT((1, 2, 3, 4)) == 1);
  static_assert(PW_ARG_COUNT((1, 2, 3), (1, 2, 3, 4)) == 2);
  static_assert(PW_ARG_COUNT((), ()) == 2);
  static_assert(PW_ARG_COUNT((-), (o)) == 2);
  static_assert(PW_ARG_COUNT((, , (, , ), ), (123, 4)) == 2);  // NOLINT
  static_assert(PW_ARG_COUNT(1, (2, 3, 4), (<5, 6>)) == 3);
}

#define SOME_VARIADIC_MACRO(...) PW_ARG_COUNT(__VA_ARGS__)

#define ANOTHER_VARIADIC_MACRO(arg, ...) SOME_VARIADIC_MACRO(__VA_ARGS__)

#define ALWAYS_ONE_ARG(...) SOME_VARIADIC_MACRO((__VA_ARGS__))

TEST(CountArgs, NestedMacros) {
  static_assert(SOME_VARIADIC_MACRO() == 0);
  static_assert(SOME_VARIADIC_MACRO(X1) == 1);
  static_assert(SOME_VARIADIC_MACRO(X1, X2) == 2);
  static_assert(SOME_VARIADIC_MACRO(X1, X2, X3) == 3);
  static_assert(SOME_VARIADIC_MACRO(X1, X2, X3, X4) == 4);
  static_assert(SOME_VARIADIC_MACRO(X1, X2, X3, X4, X5) == 5);

  static_assert(ANOTHER_VARIADIC_MACRO() == 0);
  static_assert(ANOTHER_VARIADIC_MACRO(X0) == 0);
  static_assert(ANOTHER_VARIADIC_MACRO(X0, X1) == 1);
  static_assert(ANOTHER_VARIADIC_MACRO(X0, X1, X2) == 2);
  static_assert(ANOTHER_VARIADIC_MACRO(X0, X1, X2, X3) == 3);
  static_assert(ANOTHER_VARIADIC_MACRO(X0, X1, X2, X3, X4) == 4);
  static_assert(ANOTHER_VARIADIC_MACRO(X0, X1, X2, X3, X4, X5) == 5);

  static_assert(ALWAYS_ONE_ARG() == 1);
  static_assert(ALWAYS_ONE_ARG(X0) == 1);
  static_assert(ALWAYS_ONE_ARG(X0, X1) == 1);
  static_assert(ALWAYS_ONE_ARG(X0, X1, X2) == 1);
  static_assert(ALWAYS_ONE_ARG(X0, X1, X2, X3) == 1);
  static_assert(ALWAYS_ONE_ARG(X0, X1, X2, X3, X4) == 1);
  static_assert(ALWAYS_ONE_ARG(X0, X1, X2, X3, X4, X5) == 1);
}

/* Tests all supported arg counts. This test was generated by the following
   Python 3 code:
for i in range(64 + 1):
  args = [f'X{x}' for x in range(1, i + 1)]
  print(f'  static_assert(PW_ARG_COUNT({", ".join(args)}) == {i})  // NOLINT')
*/
TEST(CountArgs, AllSupported) {
  // clang-format off
  static_assert(PW_ARG_COUNT() == 0);  // NOLINT
  static_assert(PW_ARG_COUNT(X1) == 1);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2) == 2);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3) == 3);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4) == 4);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5) == 5);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6) == 6);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7) == 7);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8) == 8);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9) == 9);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10) == 10);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11) == 11);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12) == 12);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13) == 13);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14) == 14);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15) == 15);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16) == 16);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17) == 17);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18) == 18);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19) == 19);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20) == 20);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21) == 21);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22) == 22);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23) == 23);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24) == 24);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25) == 25);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26) == 26);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27) == 27);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28) == 28);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29) == 29);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30) == 30);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31) == 31);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32) == 32);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33) == 33);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34) == 34);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35) == 35);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36) == 36);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37) == 37);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38) == 38);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39) == 39);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40) == 40);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41) == 41);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42) == 42);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43) == 43);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44) == 44);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45) == 45);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46) == 46);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47) == 47);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48) == 48);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49) == 49);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50) == 50);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51) == 51);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52) == 52);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53) == 53);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54) == 54);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55) == 55);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56) == 56);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57) == 57);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57, X58) == 58);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57, X58, X59) == 59);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57, X58, X59, X60) == 60);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57, X58, X59, X60, X61) == 61);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57, X58, X59, X60, X61, X62) == 62);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57, X58, X59, X60, X61, X62, X63) == 63);  // NOLINT
  static_assert(PW_ARG_COUNT(X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, X33, X34, X35, X36, X37, X38, X39, X40, X41, X42, X43, X44, X45, X46, X47, X48, X49, X50, X51, X52, X53, X54, X55, X56, X57, X58, X59, X60, X61, X62, X63, X64) == 64);  // NOLINT
  // clang-format on
}

TEST(DelegateByArgCount, WithoutAndWithoutArguments) {
#define TEST_SUM0() (0)
#define TEST_SUM1(a) (a)
#define TEST_SUM2(a, b) ((a) + (b))
#define TEST_SUM3(a, b, c) ((a) + (b) + (c))

  static_assert(PW_DELEGATE_BY_ARG_COUNT(TEST_SUM) == 0);
  static_assert(PW_DELEGATE_BY_ARG_COUNT(TEST_SUM, 5) == 5);
  static_assert(PW_DELEGATE_BY_ARG_COUNT(TEST_SUM, 1, 2) == 3);
  static_assert(PW_DELEGATE_BY_ARG_COUNT(TEST_SUM, 1, 2, 3) == 6);
}

}  // namespace
}  // namespace pw
