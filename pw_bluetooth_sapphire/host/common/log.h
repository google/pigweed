// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_LOG_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_LOG_H_

#include <lib/ddk/driver.h>

#include <cstddef>
#include <string>

#include "src/connectivity/bluetooth/core/bt-host/common/to_string.h"

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
// By default, the log messages use <lib/ddk/debug.h> as its backend. In this mode
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
// When the host stack code is run outside a driver log messages can be routed to stdout via printf
// instead of driver_logf. To enable this mode, call the UsePrintf() function at process start-up:
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
// <lib/ddk/debug.h>
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

// No-op function used to check consistency between format string and arguments.
[[gnu::format(printf, 1, 2)]] constexpr void CheckFormat([[maybe_unused]] const char* fmt, ...) {}

}  // namespace internal
}  // namespace bt

// This macro should be kept as small as possible to reduce binary size.
// This macro should not wrap its contents in a lambda, as it breaks logs using __FUNCTION__.
// TODO(fxbug.dev/1390): Due to limitations, |tag| is processed by printf-style formatters as a
// format string, so check that |tag| does not specify any additional args.
#define bt_log(flag, tag, fmt...)                                        \
  ::bt::LogMessage(__FILE__, __LINE__, bt::LogSeverity::flag, tag, fmt); \
  ::bt::internal::CheckFormat(tag)

#define BT_DECLARE_FAKE_DRIVER() zx_driver_rec_t __zircon_driver_rec__ = {}

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_LOG_H_
