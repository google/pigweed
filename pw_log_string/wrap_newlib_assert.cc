// Copyright 2022 The Pigweed Authors
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

// This file provides an implementation of assert that can be used with ld's
// --wrap option to wrap NEWLIB's underlying assert function. This can be used
// in cases where replacing the assert macro via include overrides isn't
// feasible, but you still need to wrap third party codes usage of assert.
//
// It redirect assert calls to a PW_LOG_LEVEL_FATAL message with the asserts
// filename, line info and failed expression.

#include <assert.h>  // __assert_func

#include "pw_log/levels.h"
#include "pw_log/options.h"
#include "pw_log_string/handler.h"

// Get the name of the wrapper function for "function",
// according to GNU ld's --wrap option.
#define WRAPPER(function) __wrap_##function

#ifdef __NEWLIB__
// newlib assert() calls __assert_func().
//
// __NEWLIB__ is defined in <_newlib_version.h> which gets included indirectly
// via <assert.h>.

// Ensure our function signature matches the real function.
extern "C" decltype(__assert_func) WRAPPER(__assert_func);

// Wrap newlib's __assert_func() to redirect assert() failures to our
// pw_log_string:handler implementation.
extern "C" void WRAPPER(__assert_func)(const char* filename,
                                       int line,
                                       const char* /* function */,
                                       const char* expr) {
  pw_log_string_HandleMessage(PW_LOG_LEVEL_FATAL,
                              PW_LOG_FLAGS,
                              PW_LOG_MODULE_NAME,
                              filename,
                              line,
                              "assert() failed: %s",
                              expr);
}

#endif  // __NEWLIB__
