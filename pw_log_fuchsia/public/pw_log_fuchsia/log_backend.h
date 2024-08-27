// Copyright 2024 The Pigweed Authors
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

#include <lib/async/dispatcher.h>

#include "pw_preprocessor/arguments.h"
#include "pw_preprocessor/compiler.h"
#include "pw_preprocessor/util.h"

PW_EXTERN_C_START

void pw_Log(int level,
            const char* module_name,
            unsigned int flags,
            const char* file_name,
            int line_number,
            const char* message,
            ...) PW_PRINTF_FORMAT(6, 7);

PW_EXTERN_C_END

#define PW_HANDLE_LOG(level, module, flags, message, ...) \
  pw_Log((level),                                         \
         (module),                                        \
         (flags),                                         \
         __FILE__,                                        \
         __LINE__,                                        \
         message PW_COMMA_ARGS(__VA_ARGS__))

// Use printf for logging. The first 2 bits of the PW_HANDLE_LOG "flags" int are
// reserved, so use the third bit.
#define PW_LOG_FLAG_USE_PRINTF 1 << 2
// When specified, the log message should not be logged. This is useful for
// disabling log levels at runtime.
#define PW_LOG_FLAG_IGNORE 1 << 3

namespace pw::log_fuchsia {

// Creates LogSink client and starts listening for interest changes on
// `dispatcher`.
void InitializeLogging(async_dispatcher_t* dispatcher);

}  // namespace pw::log_fuchsia
