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

#pragma once
#include <cstddef>
#include <string>

#include "pw_bluetooth_sapphire/internal/host/common/to_string.h"
#include "pw_log/log.h"

#pragma clang diagnostic ignored "-Wvariadic-macros"

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
//       LogMessage(__FILE__, __LINE__, LogSeverity::TRACE, "bt-host", "oops:
//       %d", foo);
//     }
//
// or using the bt_log convenience macro:
//
//     bt_log(DEBUG, "bt-host", "oops: %d", foo);
//
// DRIVER MODE:
//
// By default, the log messages use <lib/ddk/debug.h> as its backend. In this
// mode the ERROR, WARN, INFO, DEBUG and TRACE severity levels directly
// correspond to the DDK severity levels. Log levels are supplied to the kernel
// commandline, e.g. to disable INFO level and enable TRACE level messages in
// the bt-host driver use the following:
//
//     driver.bthost.log=-info,+trace
//
// In driver mode, the "tag" argument to bt_log is informational and gets
// included in the log message.
//
// (refer to
// https://fuchsia.dev/fuchsia-src/reference/kernel/kernel_cmdline#drivernamelogflags
// for all supported DDK debug log flags).
//
// PRINTF MODE:
//
// When the host stack code is run outside a driver log messages can be routed
// to stdout via printf instead of driver_logf. To enable this mode, call the
// UsePrintf() function at process start-up:
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
// Use the PW_LOG_DECLARE_FAKE_DRIVER macro for this purpose:
//
//    PW_LOG_DECLARE_FAKE_DRIVER();
//
//    int main() {
//      bt::UsePrintf(bt::LogSeverity::TRACE);
//    }

namespace bt {

// Log severity levels used by the host library, following the convention of
// <lib/ddk/debug.h>
enum class LogSeverity : int {
  // Indicates unexpected failures.
  ERROR = PW_LOG_LEVEL_ERROR,

  // Indicates a situation that is not an error but may be indicative of an
  // impending problem.
  WARN = PW_LOG_LEVEL_WARN,

  // Terse information messages for startup, shutdown, or other infrequent state
  // changes.
  INFO = PW_LOG_LEVEL_INFO,

  // Verbose messages for transactions and state changes
  DEBUG = PW_LOG_LEVEL_DEBUG,

  // Legacy. Pigweed doesn't support TRACE, so we map it to DEBUG.
  TRACE = PW_LOG_LEVEL_DEBUG,
};

constexpr size_t kNumLogSeverities = 5;

bool IsPrintfLogLevelEnabled(LogSeverity severity);

unsigned int GetPwLogFlags(LogSeverity level);

void UsePrintf(LogSeverity min_severity);

namespace internal {

// No-op function used to check consistency between format string and arguments.
PW_PRINTF_FORMAT(1, 2)
constexpr void CheckFormat([[maybe_unused]] const char* fmt, ...) {}

}  // namespace internal
}  // namespace bt

// This macro should be kept as small as possible to reduce binary size.
// This macro should not wrap its contents in a lambda, as it breaks logs using
// __FUNCTION__.
// TODO(fxbug.dev/1390): Due to limitations, |tag| is processed by printf-style
// formatters as a format string, so check that |tag| does not specify any
// additional args.
#define bt_log(level, tag, fmt...)                 \
  PW_LOG(static_cast<int>(bt::LogSeverity::level), \
         tag,                                      \
         GetPwLogFlags(bt::LogSeverity::level),    \
         fmt);                                     \
  ::bt::internal::CheckFormat(tag)
