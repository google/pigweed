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

#include "pw_preprocessor/compiler.h"
#include "pw_preprocessor/util.h"

// This is needed for testing the basic crash handler.
// TODO(pwbug/17): Replace when Pigweed config system is added.
#define PW_ASSERT_BASIC_DISABLE_NORETURN 0
#if PW_ASSERT_BASIC_DISABLE_NORETURN
#define PW_ASSERT_NORETURN
#else
#define PW_ASSERT_NORETURN PW_NO_RETURN
#endif

PW_EXTERN_C_START

// Crash, including a message with the listed attributes.
void pw_Crash(const char* file_name,
              int line_number,
              const char* function_name,
              const char* message,
              ...) PW_PRINTF_FORMAT(4, 5) PW_ASSERT_NORETURN;

PW_EXTERN_C_END

// Die with a message with many attributes included. This is the crash macro
// frontend that funnels everything into the C handler above, pw_Crash().
#define PW_HANDLE_CRASH(...) \
  pw_Crash(__FILE__, __LINE__, __func__ PW_COMMA_ARGS(__VA_ARGS__))

// Die with a message with many attributes included. This is the crash macro
// frontend that funnels everything into the C handler above, pw_Crash().
#define PW_HANDLE_ASSERT_FAILURE(condition_string, message, ...) \
  pw_Crash(__FILE__,                                             \
           __LINE__,                                             \
           __func__,                                             \
           "Check failed: " condition_string                     \
           ". " message PW_COMMA_ARGS(__VA_ARGS__))

// Sample assert failure message produced by the below implementation:
//
//   Check failed: current_sensor (=610) < new_sensor (=50). More details!
//
// Putting the value next to the operand makes the string easier to read.

// clang-format off
// This is too hairy for clang format to handle and retain readability.
#define PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE(arg_a_str,         \
                                                arg_a_val,         \
                                                comparison_op_str, \
                                                arg_b_str,         \
                                                arg_b_val,         \
                                                type_fmt,          \
                                                message, ...)      \
  pw_Crash(__FILE__,                                               \
           __LINE__,                                               \
           __func__,                                               \
           "Check failed: "                                        \
               arg_a_str " (=" type_fmt ") "                       \
               comparison_op_str " "                               \
               arg_b_str " (=" type_fmt ")"                        \
               ". " message,                                       \
            arg_a_val, arg_b_val PW_COMMA_ARGS(__VA_ARGS__))
// clang-format on
