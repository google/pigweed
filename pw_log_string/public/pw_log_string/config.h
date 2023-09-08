// Copyright 2023 The Pigweed Authors
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

#include "pw_preprocessor/arguments.h"

// User-provided header to optionally override options in this file.
#if defined(PW_LOG_STRING_CONFIG_HEADER)
#include PW_LOG_STRING_CONFIG_HEADER
#endif  // defined(PW_LOG_STRING_CONFIG_HEADER)

// Default implementation which can be overriden to adjust arguments passed to
// pw_log_string_HandleMessage.
#ifndef PW_LOG_STRING_CONFIG_HANDLE_MESSAGE
#define PW_LOG_STRING_CONFIG_HANDLE_MESSAGE(level, module, flags, ...) \
  do {                                                                 \
    pw_log_string_HandleMessage((level),                               \
                                (flags),                               \
                                (module),                              \
                                __FILE__,                              \
                                __LINE__ PW_COMMA_ARGS(__VA_ARGS__));  \
  } while (0)
#endif  // PW_LOG_STRING_CONFIG_HANDLE_MESSAGE
