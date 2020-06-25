// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "log.h"

#include <stdarg.h>

#include <algorithm>
#include <string_view>

#include <ddk/debug.h>

#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace {

// Check that BaseName strips the directory portion of a string literal path at compile time.
static_assert(internal::BaseName(nullptr) == nullptr);
static_assert(internal::BaseName("") == std::string_view());
static_assert(internal::BaseName("main.cc") == std::string_view("main.cc"));
static_assert(internal::BaseName("/main.cc") == std::string_view("main.cc"));
static_assert(internal::BaseName("../foo/bar//main.cc") == std::string_view("main.cc"));

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
  va_list args;
  va_start(args, fmt);
  std::string msg = fxl::StringVPrintf(fmt, args);
  va_end(args);

  if (IsPrintfEnabled()) {
    printf("[%s:%s:%d] %s: %s\n", tag, file, line, LogSeverityToString(severity), msg.data());
  } else {
    zxlogf_etc(LogSeverityToDdkLog(severity), "[%s:%s:%d] %s", tag, file, line, msg.data());
  }
}

void UsePrintf(LogSeverity min_severity) { g_printf_min_severity = static_cast<int>(min_severity); }

}  // namespace bt
