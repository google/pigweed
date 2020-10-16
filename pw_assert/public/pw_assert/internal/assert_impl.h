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
#pragma once

#ifndef __cplusplus
#include <stddef.h>
#endif  // __cplusplus

// Note: This file depends on the backend header already being included.

#include "pw_assert/options.h"
#include "pw_preprocessor/arguments.h"
#include "pw_preprocessor/compiler.h"

// PW_CRASH - Crash the system, with a message.
#define PW_CRASH PW_HANDLE_CRASH

// PW_CHECK - If condition evaluates to false, crash. Message optional.
#define PW_CHECK(condition, ...)                              \
  do {                                                        \
    if (!(condition)) {                                       \
      _PW_CHECK_SELECT_MACRO(                                 \
          #condition, PW_HAS_ARGS(__VA_ARGS__), __VA_ARGS__); \
    }                                                         \
  } while (0)

#define PW_DCHECK(...)            \
  do {                            \
    if (PW_ASSERT_ENABLE_DEBUG) { \
      PW_CHECK(__VA_ARGS__);      \
    }                             \
  } while (0)

// PW_D?CHECK_<type>_<comparison> macros - Binary comparison asserts.
//
// The below blocks are structured in table form, violating the 80-column
// Pigweed style, in order to make it clearer what is common and what isn't
// between the multitude of assert macro instantiations. To best view this
// section, turn off editor wrapping or make your editor wide.
//
// clang-format off

// Checks for int: LE, LT, GE, GT, EQ.
#define PW_CHECK_INT_LE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_LT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, < , argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_GE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_GT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, > , argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_EQ(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_NE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, !=, argb, int, "%d", __VA_ARGS__)

// Debug checks for int: LE, LT, GE, GT, EQ.
#define PW_DCHECK_INT_LE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_INT_LE(__VA_ARGS__)
#define PW_DCHECK_INT_LT(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_INT_LT(__VA_ARGS__)
#define PW_DCHECK_INT_GE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_INT_GE(__VA_ARGS__)
#define PW_DCHECK_INT_GT(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_INT_GT(__VA_ARGS__)
#define PW_DCHECK_INT_EQ(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_INT_EQ(__VA_ARGS__)
#define PW_DCHECK_INT_NE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_INT_NE(__VA_ARGS__)

// Checks for unsigned int: LE, LT, GE, GT, EQ.
#define PW_CHECK_UINT_LE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_LT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, < , argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_GE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_GT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, > , argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_EQ(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_NE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, !=, argb, unsigned int, "%u", __VA_ARGS__)

// Debug checks for unsigned int: LE, LT, GE, GT, EQ.
#define PW_DCHECK_UINT_LE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_UINT_LE(__VA_ARGS__)
#define PW_DCHECK_UINT_LT(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_UINT_LT(__VA_ARGS__)
#define PW_DCHECK_UINT_GE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_UINT_GE(__VA_ARGS__)
#define PW_DCHECK_UINT_GT(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_UINT_GT(__VA_ARGS__)
#define PW_DCHECK_UINT_EQ(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_UINT_EQ(__VA_ARGS__)
#define PW_DCHECK_UINT_NE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_UINT_NE(__VA_ARGS__)

// Checks for pointer: LE, LT, GE, GT, EQ, NE.
#define PW_CHECK_PTR_LE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, const void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_LT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, < , argb, const void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_GE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, const void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_GT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, > , argb, const void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_EQ(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, const void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_NE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, !=, argb, const void*, "%p", __VA_ARGS__)

// Check for pointer: NOTNULL. Use "nullptr" in C++, "NULL" in C.
#ifdef __cplusplus
#define PW_CHECK_NOTNULL(arga, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, !=, nullptr, const void*, "%p", __VA_ARGS__)
#else  // __cplusplus
#define PW_CHECK_NOTNULL(arga, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, !=, NULL, const void*, "%p", __VA_ARGS__)
#endif  // __cplusplus

// Debug checks for pointer: LE, LT, GE, GT, EQ, NE, and NOTNULL.
#define PW_DCHECK_PTR_LE(...)  if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_PTR_LE(__VA_ARGS__)
#define PW_DCHECK_PTR_LT(...)  if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_PTR_LT(__VA_ARGS__)
#define PW_DCHECK_PTR_GE(...)  if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_PTR_GE(__VA_ARGS__)
#define PW_DCHECK_PTR_GT(...)  if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_PTR_GT(__VA_ARGS__)
#define PW_DCHECK_PTR_EQ(...)  if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_PTR_EQ(__VA_ARGS__)
#define PW_DCHECK_PTR_NE(...)  if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_PTR_NE(__VA_ARGS__)
#define PW_DCHECK_NOTNULL(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_NOTNULL(__VA_ARGS__)

// Checks for float: EXACT_LE, EXACT_LT, EXACT_GE, EXACT_GT, EXACT_EQ, EXACT_NE,
// NEAR.
#define PW_CHECK_FLOAT_NEAR(arga, argb, abs_tolerance, ...) \
  _PW_CHECK_FLOAT_NEAR(arga, argb, abs_tolerance, __VA_ARGS__)
#define PW_CHECK_FLOAT_EXACT_LE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_EXACT_LT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, < , argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_EXACT_GE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_EXACT_GT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, > , argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_EXACT_EQ(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_EXACT_NE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, !=, argb, float, "%f", __VA_ARGS__)

// Debug checks for float: NEAR, EXACT_LE, EXACT_LT, EXACT_GE, EXACT_GT,
// EXACT_EQ.
#define PW_DCHECK_FLOAT_NEAR(...)     if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_FLOAT_NEAR(__VA_ARGS__)
#define PW_DCHECK_FLOAT_EXACT_LE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_FLOAT_EXACT_LE(__VA_ARGS__)
#define PW_DCHECK_FLOAT_EXACT_LT(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_FLOAT_EXACT_LT(__VA_ARGS__)
#define PW_DCHECK_FLOAT_EXACT_GE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_FLOAT_EXACT_GE(__VA_ARGS__)
#define PW_DCHECK_FLOAT_EXACT_GT(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_FLOAT_EXACT_GT(__VA_ARGS__)
#define PW_DCHECK_FLOAT_EXACT_EQ(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_FLOAT_EXACT_EQ(__VA_ARGS__)
#define PW_DCHECK_FLOAT_EXACT_NE(...) if (!(PW_ASSERT_ENABLE_DEBUG)) {} else PW_CHECK_FLOAT_EXACT_NE(__VA_ARGS__)

// clang-format on

// PW_CHECK - If condition evaluates to false, crash. Message optional.
#define PW_CHECK_OK(status, ...)                          \
  do {                                                    \
    if (status != PW_STATUS_OK) {                         \
      _PW_CHECK_OK_SELECT_MACRO(#status,                  \
                                pw_StatusString(status),  \
                                PW_HAS_ARGS(__VA_ARGS__), \
                                __VA_ARGS__);             \
    }                                                     \
  } while (0)

#define PW_DCHECK_OK(...)          \
  if (!(PW_ASSERT_ENABLE_DEBUG)) { \
  } else                           \
    PW_CHECK_OK(__VA_ARGS__)

// =========================================================================
// Implementation for PW_CHECK

// Two layers of select macros are used to enable the preprocessor to expand
// macros in the arguments to ultimately token paste the final macro name based
// on whether there are printf-style arguments.
#define _PW_CHECK_SELECT_MACRO(condition, has_args, ...) \
  _PW_CHECK_SELECT_MACRO_EXPANDED(condition, has_args, __VA_ARGS__)

// Delegate to the macro
#define _PW_CHECK_SELECT_MACRO_EXPANDED(condition, has_args, ...) \
  _PW_CHECK_HAS_MSG_##has_args(condition, __VA_ARGS__)

// PW_CHECK version 1: No message or args
#define _PW_CHECK_HAS_MSG_0(condition, ignored_arg) \
  PW_HANDLE_ASSERT_FAILURE(condition, "")

// PW_CHECK version 2: With message (and maybe args)
#define _PW_CHECK_HAS_MSG_1(condition, ...) \
  PW_HANDLE_ASSERT_FAILURE(condition, __VA_ARGS__)

// =========================================================================
// Implementation for PW_CHECK_<type>_<comparison>

// Two layers of select macros are used to enable the preprocessor to expand
// macros in the arguments to ultimately token paste the final macro name based
// on whether there are printf-style arguments.
#define _PW_CHECK_BINARY_COMPARISON_SELECT_MACRO(argument_a_str,       \
                                                 argument_a_val,       \
                                                 comparison_op_str,    \
                                                 argument_b_str,       \
                                                 argument_b_val,       \
                                                 type_fmt,             \
                                                 has_args,             \
                                                 ...)                  \
  _PW_CHECK_SELECT_BINARY_COMPARISON_MACRO_EXPANDED(argument_a_str,    \
                                                    argument_a_val,    \
                                                    comparison_op_str, \
                                                    argument_b_str,    \
                                                    argument_b_val,    \
                                                    type_fmt,          \
                                                    has_args,          \
                                                    __VA_ARGS__)

// Delegate to the macro
#define _PW_CHECK_SELECT_BINARY_COMPARISON_MACRO_EXPANDED(argument_a_str,    \
                                                          argument_a_val,    \
                                                          comparison_op_str, \
                                                          argument_b_str,    \
                                                          argument_b_val,    \
                                                          type_fmt,          \
                                                          has_args,          \
                                                          ...)               \
  _PW_CHECK_BINARY_COMPARISON_HAS_MSG_##has_args(argument_a_str,             \
                                                 argument_a_val,             \
                                                 comparison_op_str,          \
                                                 argument_b_str,             \
                                                 argument_b_val,             \
                                                 type_fmt,                   \
                                                 __VA_ARGS__)

// PW_CHECK_BINARY_COMPARISON version 1: No message or args
#define _PW_CHECK_BINARY_COMPARISON_HAS_MSG_0(argument_a_str,    \
                                              argument_a_val,    \
                                              comparison_op_str, \
                                              argument_b_str,    \
                                              argument_b_val,    \
                                              type_fmt,          \
                                              ignored_arg)       \
  PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE(argument_a_str,        \
                                          argument_a_val,        \
                                          comparison_op_str,     \
                                          argument_b_str,        \
                                          argument_b_val,        \
                                          type_fmt,              \
                                          "")

// PW_CHECK_BINARY_COMPARISON version 2: With message (and maybe args)
#define _PW_CHECK_BINARY_COMPARISON_HAS_MSG_1(argument_a_str,    \
                                              argument_a_val,    \
                                              comparison_op_str, \
                                              argument_b_str,    \
                                              argument_b_val,    \
                                              type_fmt,          \
                                              ...)               \
  PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE(argument_a_str,        \
                                          argument_a_val,        \
                                          comparison_op_str,     \
                                          argument_b_str,        \
                                          argument_b_val,        \
                                          type_fmt,              \
                                          __VA_ARGS__)

// For the binary assertions, this private macro is re-used for almost all of
// the variants. Due to limitations of C formatting, it is necessary to have
// separate macros for the types.
//
// The macro avoids evaluating the arguments multiple times at the cost of some
// macro complexity.
#define _PW_CHECK_BINARY_CMP_IMPL(                                       \
    argument_a, comparison_op, argument_b, type_decl, type_fmt, ...)     \
  do {                                                                   \
    type_decl evaluated_argument_a = (type_decl)(argument_a);            \
    type_decl evaluated_argument_b = (type_decl)(argument_b);            \
    if (!(evaluated_argument_a comparison_op evaluated_argument_b)) {    \
      _PW_CHECK_BINARY_COMPARISON_SELECT_MACRO(#argument_a,              \
                                               evaluated_argument_a,     \
                                               #comparison_op,           \
                                               #argument_b,              \
                                               evaluated_argument_b,     \
                                               type_fmt,                 \
                                               PW_HAS_ARGS(__VA_ARGS__), \
                                               __VA_ARGS__);             \
    }                                                                    \
  } while (0)

// Custom implementation for FLOAT_NEAR which is implemented through two
// underlying checks which are not trivially replaced through the use of
// FLOAT_EXACT_LE & FLOAT_EXACT_GE.
#define _PW_CHECK_FLOAT_NEAR(argument_a, argument_b, abs_tolerance, ...)       \
  do {                                                                         \
    PW_CHECK_FLOAT_EXACT_GE(abs_tolerance, 0.0f);                              \
    float evaluated_argument_a = (float)(argument_a);                          \
    float evaluated_argument_b_min = (float)(argument_b)-abs_tolerance;        \
    float evaluated_argument_b_max = (float)(argument_b) + abs_tolerance;      \
    if (!(evaluated_argument_a >= evaluated_argument_b_min)) {                 \
      _PW_CHECK_BINARY_COMPARISON_SELECT_MACRO(#argument_a,                    \
                                               evaluated_argument_a,           \
                                               ">=",                           \
                                               #argument_b " - abs_tolerance", \
                                               evaluated_argument_b_min,       \
                                               "%f",                           \
                                               PW_HAS_ARGS(__VA_ARGS__),       \
                                               __VA_ARGS__);                   \
    } else if (!(evaluated_argument_a <= evaluated_argument_b_max)) {          \
      _PW_CHECK_BINARY_COMPARISON_SELECT_MACRO(#argument_a,                    \
                                               evaluated_argument_a,           \
                                               "<=",                           \
                                               #argument_b " + abs_tolerance", \
                                               evaluated_argument_b_max,       \
                                               "%f",                           \
                                               PW_HAS_ARGS(__VA_ARGS__),       \
                                               __VA_ARGS__);                   \
    }                                                                          \
  } while (0)

// =========================================================================
// Implementation for PW_CHECK_OK

// Two layers of select macros are used to enable the preprocessor to expand
// macros in the arguments to ultimately token paste the final macro name based
// on whether there are printf-style arguments.
#define _PW_CHECK_OK_SELECT_MACRO(                    \
    status_expr_str, status_value_str, has_args, ...) \
  _PW_CHECK_OK_SELECT_MACRO_EXPANDED(                 \
      status_expr_str, status_value_str, has_args, __VA_ARGS__)

// Delegate to the macro
#define _PW_CHECK_OK_SELECT_MACRO_EXPANDED(           \
    status_expr_str, status_value_str, has_args, ...) \
  _PW_CHECK_OK_HAS_MSG_##has_args(                    \
      status_expr_str, status_value_str, __VA_ARGS__)

// PW_CHECK_OK version 1: No message or args
#define _PW_CHECK_OK_HAS_MSG_0(status_expr_str, status_value_str, ignored_arg) \
  PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE(                                     \
      status_expr_str, status_value_str, "==", "Status::OK", "OK", "%s", "")

// PW_CHECK_OK version 2: With message (and maybe args)
#define _PW_CHECK_OK_HAS_MSG_1(status_expr_str, status_value_str, ...) \
  PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE(status_expr_str,             \
                                          status_value_str,            \
                                          "==",                        \
                                          "Status::OK",                \
                                          "OK",                        \
                                          "%s",                        \
                                          __VA_ARGS__)

// Define short, usable names if requested. Note that the CHECK() macro will
// conflict with Google Log, which expects stream style logs.
#ifndef PW_ASSERT_USE_SHORT_NAMES
#define PW_ASSERT_USE_SHORT_NAMES 0
#endif

// =========================================================================
// Short name definitions (optional)

// clang-format off
#if PW_ASSERT_USE_SHORT_NAMES

// Checks that always run even in production.
#define CRASH                 PW_CRASH
#define CHECK                 PW_CHECK
#define CHECK_PTR_LE          PW_CHECK_PTR_LE
#define CHECK_PTR_LT          PW_CHECK_PTR_LT
#define CHECK_PTR_GE          PW_CHECK_PTR_GE
#define CHECK_PTR_GT          PW_CHECK_PTR_GT
#define CHECK_PTR_EQ          PW_CHECK_PTR_EQ
#define CHECK_PTR_NE          PW_CHECK_PTR_NE
#define CHECK_NOTNULL         PW_CHECK_NOTNULL
#define CHECK_INT_LE          PW_CHECK_INT_LE
#define CHECK_INT_LT          PW_CHECK_INT_LT
#define CHECK_INT_GE          PW_CHECK_INT_GE
#define CHECK_INT_GT          PW_CHECK_INT_GT
#define CHECK_INT_EQ          PW_CHECK_INT_EQ
#define CHECK_INT_NE          PW_CHECK_INT_NE
#define CHECK_UINT_LE         PW_CHECK_UINT_LE
#define CHECK_UINT_LT         PW_CHECK_UINT_LT
#define CHECK_UINT_GE         PW_CHECK_UINT_GE
#define CHECK_UINT_GT         PW_CHECK_UINT_GT
#define CHECK_UINT_EQ         PW_CHECK_UINT_EQ
#define CHECK_UINT_NE         PW_CHECK_UINT_NE
#define CHECK_FLOAT_NEAR      PW_CHECK_FLOAT_NEAR
#define CHECK_FLOAT_EXACT_LE  PW_CHECK_FLOAT_EXACT_LE
#define CHECK_FLOAT_EXACT_LT  PW_CHECK_FLOAT_EXACT_LT
#define CHECK_FLOAT_EXACT_GE  PW_CHECK_FLOAT_EXACT_GE
#define CHECK_FLOAT_EXACT_GT  PW_CHECK_FLOAT_EXACT_GT
#define CHECK_FLOAT_EXACT_EQ  PW_CHECK_FLOAT_EXACT_EQ
#define CHECK_FLOAT_EXACT_NE  PW_CHECK_FLOAT_EXACT_NE
#define CHECK_OK              PW_CHECK_OK

// Checks that are disabled if NDEBUG is not defined.
#define DCHECK                PW_DCHECK
#define DCHECK_PTR_LE         PW_DCHECK_PTR_LE
#define DCHECK_PTR_LT         PW_DCHECK_PTR_LT
#define DCHECK_PTR_GE         PW_DCHECK_PTR_GE
#define DCHECK_PTR_GT         PW_DCHECK_PTR_GT
#define DCHECK_PTR_EQ         PW_DCHECK_PTR_EQ
#define DCHECK_PTR_NE         PW_DCHECK_PTR_NE
#define DCHECK_NOTNULL        PW_DCHECK_NOTNULL
#define DCHECK_INT_LE         PW_DCHECK_INT_LE
#define DCHECK_INT_LT         PW_DCHECK_INT_LT
#define DCHECK_INT_GE         PW_DCHECK_INT_GE
#define DCHECK_INT_GT         PW_DCHECK_INT_GT
#define DCHECK_INT_EQ         PW_DCHECK_INT_EQ
#define DCHECK_INT_NE         PW_DCHECK_INT_NE
#define DCHECK_UINT_LE        PW_DCHECK_UINT_LE
#define DCHECK_UINT_LT        PW_DCHECK_UINT_LT
#define DCHECK_UINT_GE        PW_DCHECK_UINT_GE
#define DCHECK_UINT_GT        PW_DCHECK_UINT_GT
#define DCHECK_UINT_EQ        PW_DCHECK_UINT_EQ
#define DCHECK_UINT_NE        PW_DCHECK_UINT_NE
#define DCHECK_FLOAT_NEAR     PW_DCHECK_FLOAT_NEAR
#define DCHECK_FLOAT_EXACT_LT PW_DCHECK_FLOAT_EXACT_LT
#define DCHECK_FLOAT_EXACT_LE PW_DCHECK_FLOAT_EXACT_LE
#define DCHECK_FLOAT_EXACT_GT PW_DCHECK_FLOAT_EXACT_GT
#define DCHECK_FLOAT_EXACT_GE PW_DCHECK_FLOAT_EXACT_GE
#define DCHECK_FLOAT_EXACT_EQ PW_DCHECK_FLOAT_EXACT_EQ
#define DCHECK_FLOAT_EXACT_NE PW_DCHECK_FLOAT_EXACT_NE
#define DCHECK_OK             PW_DCHECK_OK

#endif  // PW_ASSERT_SHORT_NAMES
// clang-format on
