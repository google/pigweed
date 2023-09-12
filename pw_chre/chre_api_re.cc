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

#include "chre/platform/log.h"
#include "chre/util/macros.h"
#include "chre_api/chre/re.h"
#include "pw_log/log.h"
#include "pw_string/format.h"

namespace {
int ToPigweedLogLevel(enum chreLogLevel level) {
  switch (level) {
    case CHRE_LOG_ERROR:
      return PW_LOG_LEVEL_ERROR;
    case CHRE_LOG_WARN:
      return PW_LOG_LEVEL_WARN;
    case CHRE_LOG_INFO:
      return PW_LOG_LEVEL_INFO;
    case CHRE_LOG_DEBUG:
      return PW_LOG_LEVEL_DEBUG;
  }
}
}  // namespace

DLL_EXPORT void chreLog(enum chreLogLevel level,
                        const char* format_string,
                        ...) {
  char log[512];

  va_list args;
  va_start(args, format_string);
  pw::StatusWithSize status = pw::string::Format(log, format_string, args);
  PW_ASSERT(status.ok());
  va_end(args);

  PW_LOG(ToPigweedLogLevel(level), "CHRE", PW_LOG_FLAGS, "%s", log);
}
