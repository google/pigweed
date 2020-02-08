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
// =============================================================================
//
// This file describes Pigweed's public user-facing assert API.
//
// THIS API IS NOT STABLE OR COMPLETE! NEITHER FACADE NOR BACKEND API!
//
#pragma once

#include "pw_preprocessor/macro_arg_count.h"

// The pw_assert public API:
//
//   Trigger a crash with a message. Replaces LOG_FATAL() in other systems.
//   PW_CRASH(msg, ...)
//
//   Asserts the condition, crashes on failure. Equivalent to assert.
//   PW_CHECK(condition) or
//   PW_CHECK(condition, msg, ...)
//
//   In many cases an assert is a binary comparison. In those cases, using the
//   special binary assert macros below for <, <=, >, >=, == enables reporting
//   the values of the operands in addition to the string of the condition.
//
//   In all cases, the message argument is optional:
//   PW_CHECK_INT_LE(x, y) or
//   PW_CHECK_INT_LE(x, y, "Was booting %s subsystem", subsystem_name)
//
//   Binary comparison asserts for 'int' type ("%d" in format strings):
//   PW_CHECK_INT_LE  (a, b, msg, ...)  Asserts a <= b
//   PW_CHECK_INT_LT  (a, b, msg, ...)  Asserts a <  b
//   PW_CHECK_INT_GE  (a, b, msg, ...)  Asserts a >= b
//   PW_CHECK_INT_GT  (a, b, msg, ...)  Asserts a >  b
//   PW_CHECK_INT_EQ  (a, b, msg, ...)  Asserts a == b
//
//   Binary comparison asserts for 'unsigned int' type ("%u" in format strings):
//   PW_CHECK_UINT_LE (a, b, msg, ...)  Asserts a <= b
//   PW_CHECK_UINT_LT (a, b, msg, ...)  Asserts a <  b
//   PW_CHECK_UINT_GE (a, b, msg, ...)  Asserts a >= b
//   PW_CHECK_UINT_GT (a, b, msg, ...)  Asserts a >  b
//   PW_CHECK_UINT_EQ (a, b, msg, ...)  Asserts a == b
//
//   Binary comparison asserts for 'float' type ("%f" in format strings):
//   PW_CHECK_FLOAT_LE(a, b, msg, ...)  Asserts a <= b
//   PW_CHECK_FLOAT_LT(a, b, msg, ...)  Asserts a <  b
//   PW_CHECK_FLOAT_GE(a, b, msg, ...)  Asserts a >= b
//   PW_CHECK_FLOAT_GT(a, b, msg, ...)  Asserts a >  b
//   PW_CHECK_FLOAT_EQ(a, b, msg, ...)  Asserts a == b
//
//   Note: PW_CRASH is the equivalent of LOG_FATAL in other systems, where a
//   device crash is triggered with a message. In Pigweed, logging and
//   crashing/asserting are separated. There is a LOG_CRITICAL level in the
//   logging module, but it does not have side effects; for LOG_FATAL, instead
//   use this macro (PW_CRASH).
//
// The pw_assert_backend must provide these macros:
//
//   PW_HANDLE_CRASH(msg, ...)
//   PW_HANDLE_ASSERT_FAILURE(condition, msg, ...)
//   PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE(a, op, b, type_fmt, msg, ...)
//
//   The low level functionality of triggering a crash, rebooting a device,
//   collecting information, or hanging out in a while(1) loop, must be
//   provided by the underlying assert backend as part of the crash or assert
//   failure handling.
//
//   Note that for the assert failures, the handler should assume the assert
//   has already failed (the facade checks the condition before delegating).
//
#include "pw_assert_backend/assert_backend.h"

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

// PW_CHECK_<type>_<comparison> - if
// Checks for int: LE, LT, GE, GT, EQ.
#define PW_CHECK_INT_LE(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_LT(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, <, argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_GE(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_GT(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, >, argb, int, "%d", __VA_ARGS__)
#define PW_CHECK_INT_EQ(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, int, "%d", __VA_ARGS__)

// Checks for unsigned int: LE, LT, GE, GT, EQ.
#define PW_CHECK_UINT_LE(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_LT(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, <, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_GE(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_GT(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, >, argb, unsigned int, "%u", __VA_ARGS__)
#define PW_CHECK_UINT_EQ(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, unsigned int, "%u", __VA_ARGS__)

// Checks for float: LE, LT, GE, GT, EQ.
#define PW_CHECK_FLOAT_LE(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, <=, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_LT(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, <, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_GE(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, >=, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_GT(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, >, argb, float, "%f", __VA_ARGS__)
#define PW_CHECK_FLOAT_EQ(arga, argb, ...) \
  _PW_CHECK_BINARY_CMP_IMPL(arga, ==, argb, float, "%f", __VA_ARGS__)

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
                                          "");

// PW_CHECK_BINARY_COMPARISON version 2: With message (and maybe args)
//
// TODO(pwbug/117): Passing __VA_ARGS__ below should instead use
// PW_COMMA_ARGS(), but for some reason this isn't working. For now, leave it.
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
//
// TODO: Concat names with __LINE__; requires an extra layer of macros.
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

// Define short, usable names if requested. Note that the CHECK() macro will
// conflict with Google Log, which expects stream style logs.
//
// TODO(pwbug/17): Convert this to the config system when available.
#ifndef PW_ASSERT_USE_SHORT_NAMES
#define PW_ASSERT_USE_SHORT_NAMES 0
#endif

// clang-format off
#if PW_ASSERT_USE_SHORT_NAMES
#define CRASH          PW_CRASH
#define CHECK          PW_CHECK
#define CHECK          PW_CHECK
#define CHECK_INT_LE   PW_CHECK_INT_LE
#define CHECK_INT_LT   PW_CHECK_INT_LT
#define CHECK_INT_GE   PW_CHECK_INT_GE
#define CHECK_INT_GT   PW_CHECK_INT_GT
#define CHECK_INT_EQ   PW_CHECK_INT_EQ
#define CHECK_UINT_LE  PW_CHECK_UINT_LE
#define CHECK_UINT_LT  PW_CHECK_UINT_LT
#define CHECK_UINT_GE  PW_CHECK_UINT_GE
#define CHECK_UINT_GT  PW_CHECK_UINT_GT
#define CHECK_UINT_EQ  PW_CHECK_UINT_EQ
#define CHECK_FLOAT_LE PW_CHECK_FLOAT_LE
#define CHECK_FLOAT_LT PW_CHECK_FLOAT_LT
#define CHECK_FLOAT_GE PW_CHECK_FLOAT_GE
#define CHECK_FLOAT_GT PW_CHECK_FLOAT_GT
#define CHECK_FLOAT_EQ PW_CHECK_FLOAT_EQ
#endif  // PW_ASSERT_SHORT_NAMES
// clang-format on
