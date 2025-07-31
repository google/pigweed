// Copyright 2025 The Pigweed Authors
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

#include <lib/driver/logging/cpp/logger.h>

#include <cstdarg>
#include <cstdio>

#include "pw_log/levels.h"
#include "pw_log_fuchsia/log_backend.h"

namespace {

const char* LogLevelToString(int pw_level) {
  switch (pw_level) {
    case PW_LOG_LEVEL_ERROR:
      return "ERROR";
    case PW_LOG_LEVEL_WARN:
      return "WARN";
    case PW_LOG_LEVEL_INFO:
      return "INFO";
    case PW_LOG_LEVEL_DEBUG:
      return "DEBUG";
    default:
      return "UNKNOWN";
  }
}

FuchsiaLogSeverity PigweedLevelToFuchsiaSeverity(int pw_level) {
  switch (pw_level) {
    case PW_LOG_LEVEL_ERROR:
      return FUCHSIA_LOG_ERROR;
    case PW_LOG_LEVEL_WARN:
      return FUCHSIA_LOG_WARNING;
    case PW_LOG_LEVEL_INFO:
      return FUCHSIA_LOG_INFO;
    case PW_LOG_LEVEL_DEBUG:
      return FUCHSIA_LOG_DEBUG;
    default:
      return FUCHSIA_LOG_ERROR;
  }
}

}  // namespace

extern "C" void pw_Log(int level,
                       const char* module_name,
                       unsigned int flags,
                       const char* file_name,
                       int line_number,
                       const char* message,
                       ...) {
  if (flags & PW_LOG_FLAG_IGNORE) {
    return;
  }

  va_list args;
  va_start(args, message);

  if (flags & PW_LOG_FLAG_USE_PRINTF) {
    const char* last_slash = strrchr(file_name, '/');
    if (last_slash) {
      file_name = last_slash + 1;
    }
    printf("%s: [%s:%s:%d] ",
           LogLevelToString(level),
           module_name,
           file_name,
           line_number);
    vprintf(message, args);
    putchar('\n');
  } else {
    fdf::Logger::GlobalInstance()->logvf(PigweedLevelToFuchsiaSeverity(level),
                                         module_name,
                                         file_name,
                                         line_number,
                                         message,
                                         args);
  }

  va_end(args);
}

namespace pw::log_fuchsia {

void InitializeLogging(async_dispatcher_t* dispatcher) {
  // This is a no-op; we use the driver global instance.
}

}  // namespace pw::log_fuchsia
