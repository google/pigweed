// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "log.h"

#include <lib/ddk/debug.h>
#include <stdarg.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt {
namespace {

std::atomic_int g_printf_min_severity(-1);

fx_log_severity_t kDdkSeverities[kNumLogSeverities] = {
    DDK_LOG_ERROR, DDK_LOG_WARNING, DDK_LOG_INFO, DDK_LOG_DEBUG, DDK_LOG_TRACE,
};

const char* const kLogSeverityNames[kNumLogSeverities] = {
    "ERROR", "WARNING", "INFO", "DEBUG", "TRACE",
};

constexpr size_t LogSeverityToIndex(LogSeverity severity) {
  return std::min(kNumLogSeverities - 1, static_cast<size_t>(severity));
}

inline fx_log_severity_t LogSeverityToDdkLog(LogSeverity severity) {
  return kDdkSeverities[LogSeverityToIndex(severity)];
}

inline const char* LogSeverityToString(LogSeverity severity) {
  return kLogSeverityNames[LogSeverityToIndex(severity)];
}

bool IsPrintfEnabled() { return g_printf_min_severity >= 0; }

}  // namespace

bool IsLogLevelEnabled(LogSeverity severity) {
  if (IsPrintfEnabled()) {
    return static_cast<int>(severity) <= g_printf_min_severity;
  }
  return zxlog_level_enabled_etc(LogSeverityToDdkLog(severity));
}

void LogMessage(const char* file, int line, LogSeverity severity, const char* tag, const char* fmt,
                ...) {
  if (!bt::IsLogLevelEnabled(severity)) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  if (IsPrintfEnabled()) {
    std::string msg =
        bt_lib_cpp_string::StringPrintf("%s: [%s:%s:%d] %s\n", LogSeverityToString(severity), tag,
                                        bt::internal::BaseName(file), line, fmt);
    vprintf(msg.c_str(), args);
  } else {
    std::string msg = bt_lib_cpp_string::StringPrintf("[%s] %s", tag, fmt);
    zxlogvf_etc(LogSeverityToDdkLog(severity), nullptr, file, line, msg.c_str(), args);
  }
  va_end(args);
}

void UsePrintf(LogSeverity min_severity) { g_printf_min_severity = static_cast<int>(min_severity); }

}  // namespace bt
