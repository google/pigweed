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

struct LogScopeState {
  std::vector<std::string> scopes;
  std::vector<std::string> added_contexts;
};
thread_local LogScopeState g_log_scope_state;

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

std::string FormattedLogScopes() {
  std::string formatted;
  for (const auto& scope : g_log_scope_state.scopes) {
    formatted += bt_lib_cpp_string::StringPrintf("[%s]", scope.c_str());
  }
  return formatted;
}

std::string FormattedLogContexts() {
  if (g_log_scope_state.added_contexts.empty()) {
    return "";
  }

  std::string formatted;
  for (auto it = g_log_scope_state.added_contexts.begin();
       it != g_log_scope_state.added_contexts.end(); it++) {
    if (it == g_log_scope_state.added_contexts.end() - 1) {
      formatted += *it;
      break;
    }
    formatted += *it + ",";
  }
  return bt_lib_cpp_string::StringPrintf("{%s}", formatted.c_str());
}

void LogMessage(const char* file, int line, LogSeverity severity, const char* tag, const char* fmt,
                ...) {
  if (!bt::IsLogLevelEnabled(severity)) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  if (IsPrintfEnabled()) {
    std::string msg = bt_lib_cpp_string::StringPrintf(
        "%s: [%s:%s:%d]%s%s %s\n", LogSeverityToString(severity), tag, bt::internal::BaseName(file),
        line, FormattedLogContexts().c_str(), FormattedLogScopes().c_str(), fmt);
    vprintf(msg.c_str(), args);
  } else {
    std::string msg = bt_lib_cpp_string::StringPrintf(
        "[%s]%s%s %s", tag, FormattedLogContexts().c_str(), FormattedLogScopes().c_str(), fmt);

    zxlogvf_etc(LogSeverityToDdkLog(severity), nullptr, file, line, msg.c_str(), args);
  }
  va_end(args);
}

void UsePrintf(LogSeverity min_severity) { g_printf_min_severity = static_cast<int>(min_severity); }

namespace internal {

LogScopeGuard::LogScopeGuard(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::string scope = bt_lib_cpp_string::StringVPrintf(fmt, args);
  va_end(args);
  g_log_scope_state.scopes.push_back(std::move(scope));
}

LogScopeGuard::~LogScopeGuard() { g_log_scope_state.scopes.pop_back(); }

LogContextGuard::LogContextGuard(LogContext context) {
  empty_ = context.context.empty();
  if (!empty_) {
    g_log_scope_state.added_contexts.push_back(std::move(context.context));
  }
}

LogContextGuard::~LogContextGuard() {
  if (!empty_) {
    g_log_scope_state.added_contexts.pop_back();
  }
}

LogContext SaveLogContext() {
  return LogContext{bt_lib_cpp_string::StringPrintf("%s%s", FormattedLogContexts().c_str(),
                                                    FormattedLogScopes().c_str())};
}

}  // namespace internal
}  // namespace bt
