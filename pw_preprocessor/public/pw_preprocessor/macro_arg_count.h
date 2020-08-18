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
// Macros for counting the number of arguments passed to a variadic
// function-like macro.
#pragma once

#include "pw_preprocessor/boolean.h"

// PW_ARG_COUNT counts the number of arguments it was called with. It evalulates
// to an integer literal in the range 0 to 64. Counting more than 64 arguments
// is not currently supported.
//
// PW_ARG_COUNT is most commonly used to count __VA_ARGS__ in a variadic macro.
// For example, the following code counts the number of arguments passed to a
// logging macro:
//
/*   #define LOG_INFO(format, ...) {                             \
         static const int kArgCount = PW_ARG_COUNT(__VA_ARGS__); \
         SendLog(kArgCount, format, ##__VA_ARGS__);              \
       }
*/
// clang-format off
#define PW_ARG_COUNT(...)                            \
  _PW_ARG_COUNT_IMPL(__VA_ARGS__,                    \
                     64, 63, 62, 61, 60, 59, 58, 57, \
                     56, 55, 54, 53, 52, 51, 50, 49, \
                     48, 47, 46, 45, 44, 43, 42, 41, \
                     40, 39, 38, 37, 36, 35, 34, 33, \
                     32, 31, 30, 29, 28, 27, 26, 25, \
                     24, 23, 22, 21, 20, 19, 18, 17, \
                     16, 15, 14, 13, 12, 11, 10,  9, \
                      8,  7,  6,  5, 4,  3,  2,  PW_HAS_ARGS(__VA_ARGS__))

// Expands to 1 if one or more arguments are provided, 0 otherwise.
#define PW_HAS_ARGS(...) PW_NOT(PW_EMPTY_ARGS(__VA_ARGS__))

// Expands to 0 if one or more arguments are provided, 1 otherwise. This
// approach is from Jens Gustedt's blog:
//   https://gustedt.wordpress.com/2010/06/08/detect-empty-macro-arguments/
//
// Normally, with a standard-compliant C preprocessor, it's impossible to tell
// whether a variadic macro was called with no arguments or with one argument.
// A macro invoked with no arguments is actually passed one empty argument.
//
// This macro works by checking for the presence of a comma in four situations.
// These situations give the following information about __VA_ARGS__:
//
//   1. It is two or more variadic arguments.
//   2. It expands to one argument surrounded by parentheses.
//   3. It is a function-like macro that produces a comma when invoked.
//   4. It does not interfere with calling a macro when placed between it and
//      parentheses.
//
// If a comma is not present in 1, 2, 3, but is present in 4, then __VA_ARGS__
// is empty. For this case (0001), and only this case, a corresponding macro
// that expands to a comma is defined. The presence of this comma determines
// whether any arguments were passed in.
//
// C++20 introduces __VA_OPT__, which would greatly simplify this macro.
#define PW_EMPTY_ARGS(...)                                            \
  _PW_HAS_NO_ARGS(_PW_HAS_COMMA(__VA_ARGS__),                          \
                  _PW_HAS_COMMA(_PW_MAKE_COMMA_IF_CALLED __VA_ARGS__), \
                  _PW_HAS_COMMA(__VA_ARGS__()),                        \
                  _PW_HAS_COMMA(_PW_MAKE_COMMA_IF_CALLED __VA_ARGS__()))

#define _PW_HAS_COMMA(...)                                           \
  _PW_ARG_COUNT_IMPL(__VA_ARGS__,                                    \
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)

#define _PW_ARG_COUNT_IMPL(a64, a63, a62, a61, a60, a59, a58, a57, \
                           a56, a55, a54, a53, a52, a51, a50, a49, \
                           a48, a47, a46, a45, a44, a43, a42, a41, \
                           a40, a39, a38, a37, a36, a35, a34, a33, \
                           a32, a31, a30, a29, a28, a27, a26, a25, \
                           a24, a23, a22, a21, a20, a19, a18, a17, \
                           a16, a15, a14, a13, a12, a11, a10, a09, \
                           a08, a07, a06, a05, a04, a03, a02, a01, \
                           count, ...)                             \
  count

// clang-format on
#define _PW_HAS_NO_ARGS(a1, a2, a3, a4) \
  _PW_HAS_COMMA(_PW_PASTE_RESULTS(a1, a2, a3, a4))
#define _PW_PASTE_RESULTS(a1, a2, a3, a4) _PW_HAS_COMMA_CASE_##a1##a2##a3##a4
#define _PW_HAS_COMMA_CASE_0001 ,
#define _PW_MAKE_COMMA_IF_CALLED(...) ,

// Expands to a comma followed by __VA_ARGS__, if __VA_ARGS__ is non-empty.
// Otherwise, expands to nothing. If the final argument is empty, it is omitted.
// This is useful when passing __VA_ARGS__ to a variadic function or template
// parameter list, since it removes the extra comma when no arguments are
// provided. PW_COMMA_ARGS must NOT be used when invoking a macro from another
// macro.
//
// This is a more flexible, standard-compliant version of ##__VA_ARGS__. Unlike
// ##__VA_ARGS__, this can be used to eliminate an unwanted comma when
// __VA_ARGS__ expands to an empty argument because an outer macro was called
// with __VA_ARGS__ instead of ##__VA_ARGS__.
//
// PW_COMMA_ARGS must NOT be used to conditionally include a comma when invoking
// a macro from another macro. PW_COMMA_ARGS only functions correctly when the
// macro expands to C or C++ code! Using it with intermediate macros can result
// in out-of-order parameters. When invoking one macro from another, simply pass
// __VA_ARGS__. Only the final macro that expands to C/C++ code should use
// PW_COMMA_ARGS.
//
// For example, the following does NOT work:
/*
     #define MY_MACRO(fmt, ...) \
         NESTED_MACRO(fmt PW_COMMA_ARGS(__VA_ARGS__))  // BAD! Do not do this!
*/
// Instead, only use PW_COMMA_ARGS when the macro expands to C/C++ code:
/*
     #define MY_MACRO(fmt, ...) \
         NESTED_MACRO(fmt, __VA_ARGS__)  // Pass __VA_ARGS__ to nested macros

     #define NESTED_MACRO(fmt, ...) \
         printf(fmt PW_COMMA_ARGS(__VA_ARGS__))  // PW_COMMA_ARGS is OK here
*/
#define PW_COMMA_ARGS(...) \
  _PW_COMMA_ARGS(PW_ARG_COUNT(__VA_ARGS__), __VA_ARGS__)
#define _PW_COMMA_ARGS(count, ...) _PW_COMMA_ARGS_X(count, __VA_ARGS__)
#define _PW_COMMA_ARGS_X(count, ...) _PW_COMMA_ARGS_##count(__VA_ARGS__)

// Expand PW_COMMA_ARGS to a macro for each number of arguments. This makes it
// possible to omit the final argument if it is empty. For example, for the
// following macro:
//
//   #define MY_MACRO(...) SomeFunction(true PW_COMMA_ARGS(__VA_ARGS__))
//
// both MY_MACRO(1, 2) and MY_MACRO(1, 2, ) expand to SomeFunction(true, 1, 2).
//
// These macros were generated using the following Python code:
//
//   for i in range(2, 33):
//     args = ', '.join(f'a{arg}' for arg in range(1, i))
//     print(f'#define _PW_COMMA_ARGS_{i}({args}, a{i}) '
//           f', {args} _PW_COMMA_ARGS_1(a{i})')
//
// clang-format off
#define _PW_COMMA_ARGS_2(a1, a2) , a1 _PW_COMMA_ARGS_1(a2)
#define _PW_COMMA_ARGS_3(a1, a2, a3) , a1, a2 _PW_COMMA_ARGS_1(a3)
#define _PW_COMMA_ARGS_4(a1, a2, a3, a4) , a1, a2, a3 _PW_COMMA_ARGS_1(a4)
#define _PW_COMMA_ARGS_5(a1, a2, a3, a4, a5) , a1, a2, a3, a4 _PW_COMMA_ARGS_1(a5)
#define _PW_COMMA_ARGS_6(a1, a2, a3, a4, a5, a6) , a1, a2, a3, a4, a5 _PW_COMMA_ARGS_1(a6)
#define _PW_COMMA_ARGS_7(a1, a2, a3, a4, a5, a6, a7) , a1, a2, a3, a4, a5, a6 _PW_COMMA_ARGS_1(a7)
#define _PW_COMMA_ARGS_8(a1, a2, a3, a4, a5, a6, a7, a8) , a1, a2, a3, a4, a5, a6, a7 _PW_COMMA_ARGS_1(a8)
#define _PW_COMMA_ARGS_9(a1, a2, a3, a4, a5, a6, a7, a8, a9) , a1, a2, a3, a4, a5, a6, a7, a8 _PW_COMMA_ARGS_1(a9)
#define _PW_COMMA_ARGS_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) , a1, a2, a3, a4, a5, a6, a7, a8, a9 _PW_COMMA_ARGS_1(a10)
#define _PW_COMMA_ARGS_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10 _PW_COMMA_ARGS_1(a11)
#define _PW_COMMA_ARGS_12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 _PW_COMMA_ARGS_1(a12)
#define _PW_COMMA_ARGS_13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12 _PW_COMMA_ARGS_1(a13)
#define _PW_COMMA_ARGS_14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13 _PW_COMMA_ARGS_1(a14)
#define _PW_COMMA_ARGS_15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14 _PW_COMMA_ARGS_1(a15)
#define _PW_COMMA_ARGS_16(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15 _PW_COMMA_ARGS_1(a16)
#define _PW_COMMA_ARGS_17(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16 _PW_COMMA_ARGS_1(a17)
#define _PW_COMMA_ARGS_18(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17 _PW_COMMA_ARGS_1(a18)
#define _PW_COMMA_ARGS_19(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18 _PW_COMMA_ARGS_1(a19)
#define _PW_COMMA_ARGS_20(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19 _PW_COMMA_ARGS_1(a20)
#define _PW_COMMA_ARGS_21(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20 _PW_COMMA_ARGS_1(a21)
#define _PW_COMMA_ARGS_22(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21 _PW_COMMA_ARGS_1(a22)
#define _PW_COMMA_ARGS_23(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22 _PW_COMMA_ARGS_1(a23)
#define _PW_COMMA_ARGS_24(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23 _PW_COMMA_ARGS_1(a24)
#define _PW_COMMA_ARGS_25(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24 _PW_COMMA_ARGS_1(a25)
#define _PW_COMMA_ARGS_26(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25 _PW_COMMA_ARGS_1(a26)
#define _PW_COMMA_ARGS_27(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26 _PW_COMMA_ARGS_1(a27)
#define _PW_COMMA_ARGS_28(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27 _PW_COMMA_ARGS_1(a28)
#define _PW_COMMA_ARGS_29(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28 _PW_COMMA_ARGS_1(a29)
#define _PW_COMMA_ARGS_30(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29 _PW_COMMA_ARGS_1(a30)
#define _PW_COMMA_ARGS_31(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30 _PW_COMMA_ARGS_1(a31)
#define _PW_COMMA_ARGS_32(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, a32) , a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31 _PW_COMMA_ARGS_1(a32)
// clang-format on

#define _PW_COMMA_ARGS_0()

// If there is one argument, make a comma only if the argument is not empty.
#define _PW_COMMA_ARGS_1(arg) _PW_SKIP_COMMA(PW_EMPTY_ARGS(arg)) arg

#define _PW_SKIP_COMMA(no_comma) _PW_SKIP_COMMA_X(no_comma)
#define _PW_SKIP_COMMA_X(no_comma) _PW_SKIP_COMMA_##no_comma
#define _PW_SKIP_COMMA_1
#define _PW_SKIP_COMMA_0 ,

// Allows calling a different function-like macro based on the number of
// arguments.  For example:
//
//   #define ARG_PRINT(...)  PW_DELEGATE_BY_ARG_COUNT(_ARG_PRINT, __VA_ARGS__)
//   #define _ARG_PRINT1(a)        LOG_INFO("1 arg: %s", a)
//   #define _ARG_PRINT2(a, b)     LOG_INFO("2 args: %s, %s", a, b)
//   #define _ARG_PRINT3(a, b, c)  LOG_INFO("3 args: %s, %s, %s", a, b, c)
//
// This can the be called in code:
//    ARG_PRINT("a");            // Outputs: 1 arg: a
//    ARG_PRINT("a", "b");       // Outputs: 2 arg: a, b
//    ARG_PRINT("a", "b", "c");  // Outputs: 3 arg: a, b, c
//
#define PW_DELEGATE_BY_ARG_COUNT(func, ...) \
  _PW_DELEGATE_BY_ARG_COUNT(func, PW_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)
#define _PW_DELEGATE_BY_ARG_COUNT_EXPANDED(name, n) name##n
#define _PW_DELEGATE_BY_ARG_COUNT(name, n) \
  _PW_DELEGATE_BY_ARG_COUNT_EXPANDED(name, n)
