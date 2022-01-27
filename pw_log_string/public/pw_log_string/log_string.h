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

#include "pw_preprocessor/arguments.h"
#include "pw_preprocessor/compiler.h"
#include "pw_preprocessor/util.h"

PW_EXTERN_C_START

// Log a message with the listed attributes.
void pw_log_string_HandleMessage(int level,
                                 unsigned int flags,
                                 const char* module_name,
                                 const char* file_name,
                                 int line_number,
                                 const char* message,
                                 ...) PW_PRINTF_FORMAT(6, 7);

PW_EXTERN_C_END

// Log a message with many attributes included. This is a backend implementation
// for the logging facade in pw_log/log.h.
//
// This is the log macro frontend that funnels everything into the C handler
// above, pw_log_string_HandleMessage. It's not efficient at the callsite, since
// it passes many arguments.
#define PW_HANDLE_LOG(level, flags, message, ...)                    \
  do {                                                               \
    pw_log_string_HandleMessage((level),                             \
                                (flags),                             \
                                PW_LOG_MODULE_NAME,                  \
                                __FILE__,                            \
                                __LINE__,                            \
                                message PW_COMMA_ARGS(__VA_ARGS__)); \
  } while (0)
