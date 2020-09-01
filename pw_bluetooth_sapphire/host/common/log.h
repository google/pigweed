// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_LOG_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_LOG_H_

#include <cstddef>
#include <string>

#include <ddk/driver.h>

// Logging utilities for the host library. This provides a common abstraction
// over Zircon DDK debug utilities (used when the host stack code runs in a
// driver) and printf (when it's used in unit tests and command-line tools).
//
// USAGE:
//
// Functions have been provided to check if logging has been enabled at a
// certain severity and to log a message using a tag, file name, and line
// number:
//
//     if (IsLogLevelEnabled(LogSeverity::TRACE)) {
//       LogMessage(__FILE__, __LINE__, LogSeverity::TRACE, "bt-host", "oops: %d", foo);
//     }
//
// or using the bt_log convenience macro:
//
//     bt_log(DEBUG, "bt-host", "oops: %d", foo);
//
// DRIVER MODE:
//
// By default, the log messages use <ddk/debug.h> as its backend. In this mode
// the ERROR, WARN, INFO, DEBUG and TRACE severity levels directly correspond to
// the DDK severity levels. Log levels are supplied to the kernel commandline,
// e.g. to disable INFO level and enable TRACE level messages in the bt-host
// driver use the following:
//
//     driver.bthost.log=-info,+trace
//
// In driver mode, the "tag" argument to bt_log is informational and gets
// included in the log message.
//
// (refer to https://fuchsia.dev/fuchsia-src/reference/kernel/kernel_cmdline#drivernamelogflags
// for all supported DDK debug log flags).
//
// PRINTF MODE:
//
// When the host stack code is run outside a driver (e.g. bt-host-unittests) log
// messages can be routed to stdout via printf instead of driver_logf. To enable
// this mode, call the UsePrintf() function at process start-up:
//
//    int main() {
//      bt::UsePrintf(bt::LogSeverity::ERROR);
//
//      ...do stuff...
//
//      return 0;
//    }
//
// The |min_severity| parameter determines the smallest severity level that will
// be allowed. For example, passing LogSeverity::INFO will enable INFO, WARN,
// and ERROR severity levels.
//
// The UsePrintf() function is NOT thread-safe. This should be called EARLY
// and ONLY ONCE during initialization. Once the printf mode is enabled it
// cannot be toggled back to driver mode.
//
// CAVEATS:
//
// Since the logging mode is determined at run-time and not compile-time (due
// to build dependency reasons) users of these utilities will need to link a
// symbol for |__zircon_driver_rec__|. While this symbol will remain unused in
// printf-mode it is needed to pass compilation if the target is not a driver.
// Use the BT_DECLARE_FAKE_DRIVER macro for this purpose:
//
//    BT_DECLARE_FAKE_DRIVER();
//
//    int main() {
//      bt::UsePrintf(bt::LogSeverity::TRACE);
//    }

namespace bt {

// Log severity levels used by the host library, following the convention of
// <ddk/debug.h>
enum class LogSeverity {
  // Indicates unexpected failures.
  ERROR = 0,

  // Indicates a situation that is not an error but may be indicative of an
  // impending problem.
  WARN = 1,

  // Terse information messages for startup, shutdown, or other infrequent state
  // changes.
  INFO = 2,

  // Verbose messages for transactions and state changes
  DEBUG = 3,

  // Very verbose messages.
  TRACE = 4,
};

constexpr size_t kNumLogSeverities = 5;

bool IsLogLevelEnabled(LogSeverity severity);
[[gnu::format(printf, 5, 6)]] void LogMessage(const char* file, int line, LogSeverity severity,
                                              const char* tag, const char* fmt, ...);

void UsePrintf(LogSeverity min_severity);

struct LogContext {
  std::string context;
};

namespace internal {

// Returns the part of a path following the final '/', or the whole path if there is no '/'.
constexpr const char* BaseName(const char* path) {
  for (const char* c = path; c && (*c != '\0'); c++) {
    if (*c == '/') {
      path = c + 1;
    }
  }
  return path;
}

// LogScope is a helper class that manages the lifetime of a log scope in the global thread-local
// list of log scopes. LogScope should only be created through the bt_log_scope() macro in order to
// guarantee that the the correct scope is removed when a LogScope is destroyed.
class LogScopeGuard final {
 public:
  // Push log scope.
  [[gnu::format(printf, 2, 3)]] explicit LogScopeGuard(const char* fmt, ...);

  // Pop log scope.
  ~LogScopeGuard();
};

// Like LogScopeGuard, but for LogContext.
class LogContextGuard final {
 public:
  // Restore log context.
  explicit LogContextGuard(LogContext);

  // Pop log context.
  ~LogContextGuard();

 private:
  bool empty_;
};

// Returns a snapshot of the current registered log scopes that can be restored with
// LogContextGuard::LogContextGuard(LogContext).
LogContext SaveLogContext();

}  // namespace internal
}  // namespace bt

#define bt_log(flag, tag, fmt...)                                                                  \
  do {                                                                                             \
    if (bt::IsLogLevelEnabled(bt::LogSeverity::flag)) {                                            \
      bt::LogMessage(bt::internal::BaseName(__FILE__), __LINE__, bt::LogSeverity::flag, tag, fmt); \
    }                                                                                              \
  } while (0)

#define BT_DECLARE_FAKE_DRIVER() zx_driver_rec_t __zircon_driver_rec__ = {};

// Convenience macro for printf-style formatting of an object with a ToString()
// method e.g.:
//   bt_log(INFO, "tag", "foo happened: %s", bt_str(id));
#define bt_str(id) ((id).ToString().c_str())

#define __BT_CONCAT_(x, y) x##y
// This level of indirection is required for concatenating the results of macros.
#define __BT_CONCAT(x, y) __BT_CONCAT_(x, y)

// bt_log_scope() is a helper macro for defining a uniquely named LogScope variable that will remove
// the log scope at the end of the current scope. capture_log_context() and add_parent_context() can
// be used to save and restore scopes in async callbacks.
//
// Example:
//
// void MyFunction(int value) {
//   bt_log_scope("MyFunction value=%d", value);
//   bt_log(INFO, "tag", "A");
//   auto callback = [ctx = capture_log_context()] {
//     add_parent_context(ctx);
//     bt_log(INFO, "tag", "B");
//   };
//   callback();
//   async::PostTask(async_get_default_dispatcher(), callback);
// }
//
// Myfunction(5) will log the following:
// [tag:file.cc:line][MyFunction value=5] A
// [tag:file.cc:line]{[MyFunction value=5]}[MyFunction value=5] B
// [tag:file.cc:line]{[MyFunction value=5]} B
#define bt_log_scope(fmt...) bt::internal::LogScopeGuard __BT_CONCAT(log_scope_, __LINE__)(fmt)

// Returns a LogContext containing all of the current scopes and contexts.
#define capture_log_context() bt::internal::SaveLogContext()

// Prepend a saved context to the current context to indicate causality (e.g. in an async callback).
#define add_parent_context(ctx) bt::internal::LogContextGuard __BT_CONCAT(log_ctx_, __LINE__)(ctx)

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_LOG_H_
