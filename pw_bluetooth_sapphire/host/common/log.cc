// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "log.h"

#include <stdarg.h>

#include <string_view>

#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

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
  return g_printf_min_severity != -1 && static_cast<int>(severity) >= g_printf_min_severity;
}

unsigned int GetPwLogFlags(LogSeverity level) {
  if (g_printf_min_severity == -1) {
    return 0;
  }
  return IsPrintfLogLevelEnabled(level) ? PW_LOG_FLAG_USE_PRINTF : PW_LOG_FLAG_IGNORE;
}

void UsePrintf(LogSeverity min_severity) { g_printf_min_severity = static_cast<int>(min_severity); }

}  // namespace bt
