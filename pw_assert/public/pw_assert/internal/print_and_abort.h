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
#pragma once

#include <stdio.h>
#include <stdlib.h>

#include "pw_assert/config.h"

#if PW_ASSERT_ENABLE_DEBUG
#define _PW_ASSERT_DEBUG_MACRO "or PW_DASSERT() "
#else
#define _PW_ASSERT_DEBUG_MACRO
#endif  // PW_ASSERT_ENABLE_DEBUG

#ifdef __GNUC__
#define _PW_ASSERT_ABORT_FUNCTION __PRETTY_FUNCTION__
#else
#define _PW_ASSERT_ABORT_FUNCTION __func__
#endif  // __GNUC__

// This assert implementation prints the file path, line number, and assert
// expression using printf. Uses ANSI escape codes for colors.
//
// This is done with single printf to work better in multithreaded enironments.
#define PW_ASSERT_HANDLE_FAILURE(expression)           \
  fflush(stdout);                                      \
  fprintf(stderr,                                      \
          "\033[41m\033[37m\033[1m%s:%d:\033[0m "      \
          "\033[1mPW_ASSERT() " _PW_ASSERT_DEBUG_MACRO \
          "\033[31mFAILED!\033[0m\n\n"                 \
          "  FAILED ASSERTION\n\n"                     \
          "    %s\n\n"                                 \
          "  FILE & LINE\n\n"                          \
          "    %s:%d\n\n"                              \
          "  FUNCTION\n\n"                             \
          "    %s\n\n",                                \
          __FILE__,                                    \
          __LINE__,                                    \
          expression,                                  \
          __FILE__,                                    \
          __LINE__,                                    \
          _PW_ASSERT_ABORT_FUNCTION);                  \
  fflush(stderr);                                      \
  abort()
