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

#include "pw_bluetooth_sapphire/internal/host/common/log.h"

#include <cpp-string/string_printf.h>
#include <stdarg.h>

#include <atomic>
#include <memory>
#include <string_view>

// The USE_PRINTF flag only exists on Fuchsia.
#ifndef PW_LOG_FLAG_USE_PRINTF
#define PW_LOG_FLAG_USE_PRINTF 0
#endif

// The IGNORE flag only exists on Fuchsia.
#ifndef PW_LOG_FLAG_IGNORE
#define PW_LOG_FLAG_IGNORE 0
#endif

namespace bt {
namespace {

std::atomic_int g_printf_min_severity(-1);

}  // namespace

bool IsPrintfLogLevelEnabled(LogSeverity severity) {
  return g_printf_min_severity != -1 &&
         static_cast<int>(severity) >= g_printf_min_severity;
}

unsigned int GetPwLogFlags(LogSeverity level) {
  if (g_printf_min_severity == -1) {
    return 0;
  }
  return IsPrintfLogLevelEnabled(level) ? PW_LOG_FLAG_USE_PRINTF
                                        : PW_LOG_FLAG_IGNORE;
}

void UsePrintf(LogSeverity min_severity) {
  g_printf_min_severity = static_cast<int>(min_severity);
}

}  // namespace bt
