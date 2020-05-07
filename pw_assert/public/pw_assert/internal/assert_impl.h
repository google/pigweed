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

// Note: This file depends on the backend header already being included.

#include "pw_preprocessor/macro_arg_count.h"

// Define PW_ASSERT_ENABLE_DCHECK, which controls whether DCHECKs are enabled.
#if !defined(PW_ASSERT_ENABLE_DCHECK)
#if defined(NDEBUG)
// Release mode; remove all DCHECK*() asserts.
#define PW_ASSERT_ENABLE_DCHECK 0
#else
// Debug mode; keep all DCHECK*() asserts.
#define PW_ASSERT_ENABLE_DCHECK 1
#endif  // defined (NDEBUG)
#endif  // !defined(PW_ASSERT_ENABLE_DCHECK)

// PW_CRASH - Crash the system, with a message.
#define PW_CRASH(message, ...) \
  PW_HANDLE_CRASH(message PW_COMMA_ARGS(__VA_ARGS__))

// PW_CHECK - If condition evaluates to false, crash. Message optional.
#define PW_CHECK(condition, ...)                              \
  do {                                                        \
    if (!(condition)) {                                       \
      _PW_CHECK_SELECT_MACRO(                                 \
          #condition, PW_HAS_ARGS(__VA_ARGS__), __VA_ARGS__); \
    }                                                         \
  } while (0)

#define PW_DCHECK(...)         \
  if (PW_ASSERT_ENABLE_DCHECK) \
  PW_CHECK(__VA_ARGS__)

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
#define PW_DCHECK_INT_LE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_INT_LE(__VA_ARGS__)
#define PW_DCHECK_INT_LT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_INT_LT(__VA_ARGS__)
#define PW_DCHECK_INT_GE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_INT_GE(__VA_ARGS__)
#define PW_DCHECK_INT_GT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_INT_GT(__VA_ARGS__)
#define PW_DCHECK_INT_EQ(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_INT_EQ(__VA_ARGS__)
#define PW_DCHECK_INT_NE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_INT_NE(__VA_ARGS__)

// Checks for unsigned int: LE, LT, GE, GT, EQ.
#define PW_CHECK_UINT_LE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_LT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, < , argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_GE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_GT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, > , argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_EQ(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_NE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, !=, argb, unsigned int, "%u", __VA_ARGS__)

// Debug checks for unsigned int: LE, LT, GE, GT, EQ.
#define PW_DCHECK_UINT_LE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_UINT_LE(__VA_ARGS__)
#define PW_DCHECK_UINT_LT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_UINT_LT(__VA_ARGS__)
#define PW_DCHECK_UINT_GE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_UINT_GE(__VA_ARGS__)
#define PW_DCHECK_UINT_GT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_UINT_GT(__VA_ARGS__)
#define PW_DCHECK_UINT_EQ(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_UINT_EQ(__VA_ARGS__)
#define PW_DCHECK_UINT_NE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_UINT_NE(__VA_ARGS__)

// Checks for pointer: LE, LT, GE, GT, EQ, NE.
#define PW_CHECK_PTR_LE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_LT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, < , argb, void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_GE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_GT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, > , argb, void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_EQ(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, void*, "%p", __VA_ARGS__)
#define PW_CHECK_PTR_NE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, !=, argb, void*, "%p", __VA_ARGS__)

// For implementation simplicity, re-use PTR_NE for NOTNULL.
#define PW_CHECK_NOTNULL(arga, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, !=, NULL, void*, "%p", __VA_ARGS__)

// Debug checks for pointer: LE, LT, GE, GT, EQ, NE.
#define PW_DCHECK_PTR_LE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_PTR_LE(__VA_ARGS__)
#define PW_DCHECK_PTR_LT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_PTR_LT(__VA_ARGS__)
#define PW_DCHECK_PTR_GE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_PTR_GE(__VA_ARGS__)
#define PW_DCHECK_PTR_GT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_PTR_GT(__VA_ARGS__)
#define PW_DCHECK_PTR_EQ(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_PTR_EQ(__VA_ARGS__)
#define PW_DCHECK_PTR_NE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_PTR_NE(__VA_ARGS__)

// For implementation simplicity, re-use PTR_NE for NOTNULL.
#define PW_DCHECK_NOTNULL(...) \
  if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_NOTNULL(__VA_ARGS__)

// Checks for float: LE, LT, GE, GT, EQ.
#define PW_CHECK_FLOAT_LE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_LT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, < , argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_GE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_GT(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, > , argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_EQ(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_NE(arga, argb, ...) _PW_CHECK_BINARY_CMP_IMPL(arga, !=, argb, float, "%f", __VA_ARGS__)

// Debug checks for float: LE, LT, GE, GT, EQ.
#define PW_DCHECK_FLOAT_LE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_FLOAT_LE(__VA_ARGS__)
#define PW_DCHECK_FLOAT_LT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_FLOAT_LT(__VA_ARGS__)
#define PW_DCHECK_FLOAT_GE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_FLOAT_GE(__VA_ARGS__)
#define PW_DCHECK_FLOAT_GT(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_FLOAT_GT(__VA_ARGS__)
#define PW_DCHECK_FLOAT_EQ(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_FLOAT_EQ(__VA_ARGS__)
#define PW_DCHECK_FLOAT_NE(...) if (PW_ASSERT_ENABLE_DCHECK) PW_CHECK_FLOAT_NE(__VA_ARGS__)

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

#define PW_DCHECK_OK(...)      \
  if (PW_ASSERT_ENABLE_DCHECK) \
  PW_CHECK_OK(__VA_ARGS__)

// =========================================================================
// Implementation for PW_CHECK

// TODO: Explain why we must expand another time.
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

// TODO: Explain why we must expand another time.
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

// For the binary assertions, this private macro is re-used for all the
// variants. Due to limitations of C formatting, it is necessary to have
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

// =========================================================================
// Implementation for PW_CHECK_OK

// TODO: Explain why we must expand another time.
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
#define CRASH           PW_CRASH
#define CHECK           PW_CHECK
#define CHECK_PTR_LE    PW_CHECK_PTR_LE
#define CHECK_PTR_LT    PW_CHECK_PTR_LT
#define CHECK_PTR_GE    PW_CHECK_PTR_GE
#define CHECK_PTR_GT    PW_CHECK_PTR_GT
#define CHECK_PTR_EQ    PW_CHECK_PTR_EQ
#define CHECK_PTR_NE    PW_CHECK_PTR_NE
#define CHECK_NOTNULL   PW_CHECK_NOTNULL
#define CHECK_INT_LE    PW_CHECK_INT_LE
#define CHECK_INT_LT    PW_CHECK_INT_LT
#define CHECK_INT_GE    PW_CHECK_INT_GE
#define CHECK_INT_GT    PW_CHECK_INT_GT
#define CHECK_INT_EQ    PW_CHECK_INT_EQ
#define CHECK_INT_NE    PW_CHECK_INT_NE
#define CHECK_UINT_LE   PW_CHECK_UINT_LE
#define CHECK_UINT_LT   PW_CHECK_UINT_LT
#define CHECK_UINT_GE   PW_CHECK_UINT_GE
#define CHECK_UINT_GT   PW_CHECK_UINT_GT
#define CHECK_UINT_EQ   PW_CHECK_UINT_EQ
#define CHECK_UINT_NE   PW_CHECK_UINT_NE
#define CHECK_FLOAT_LE  PW_CHECK_FLOAT_LE
#define CHECK_FLOAT_LT  PW_CHECK_FLOAT_LT
#define CHECK_FLOAT_GE  PW_CHECK_FLOAT_GE
#define CHECK_FLOAT_GT  PW_CHECK_FLOAT_GT
#define CHECK_FLOAT_EQ  PW_CHECK_FLOAT_EQ
#define CHECK_FLOAT_NE  PW_CHECK_FLOAT_NE
#define CHECK_OK        PW_CHECK_OK

// Checks that are disabled if NDEBUG is not defined.
#define DCHECK          PW_DCHECK
#define DCHECK_PTR_LE   PW_DCHECK_PTR_LE
#define DCHECK_PTR_LT   PW_DCHECK_PTR_LT
#define DCHECK_PTR_GE   PW_DCHECK_PTR_GE
#define DCHECK_PTR_GT   PW_DCHECK_PTR_GT
#define DCHECK_PTR_EQ   PW_DCHECK_PTR_EQ
#define DCHECK_PTR_NE   PW_DCHECK_PTR_NE
#define DCHECK_NOTNULL  PW_DCHECK_NOTNULL
#define DCHECK_INT_LE   PW_DCHECK_INT_LE
#define DCHECK_INT_LT   PW_DCHECK_INT_LT
#define DCHECK_INT_GE   PW_DCHECK_INT_GE
#define DCHECK_INT_GT   PW_DCHECK_INT_GT
#define DCHECK_INT_EQ   PW_DCHECK_INT_EQ
#define DCHECK_INT_NE   PW_DCHECK_INT_NE
#define DCHECK_UINT_LE  PW_DCHECK_UINT_LE
#define DCHECK_UINT_LT  PW_DCHECK_UINT_LT
#define DCHECK_UINT_GE  PW_DCHECK_UINT_GE
#define DCHECK_UINT_GT  PW_DCHECK_UINT_GT
#define DCHECK_UINT_EQ  PW_DCHECK_UINT_EQ
#define DCHECK_UINT_NE  PW_DCHECK_UINT_NE
#define DCHECK_FLOAT_LE PW_DCHECK_FLOAT_LE
#define DCHECK_FLOAT_LT PW_DCHECK_FLOAT_LT
#define DCHECK_FLOAT_GE PW_DCHECK_FLOAT_GE
#define DCHECK_FLOAT_GT PW_DCHECK_FLOAT_GT
#define DCHECK_FLOAT_EQ PW_DCHECK_FLOAT_EQ
#define DCHECK_FLOAT_NE PW_DCHECK_FLOAT_NE
#define DCHECK_OK       PW_DCHECK_OK

#endif  // PW_ASSERT_SHORT_NAMES
// clang-format on
