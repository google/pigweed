// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include <ddk/driver.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/lib/fxl/command_line.h"
#include "src/lib/fxl/log_settings_command_line.h"
#include "src/lib/fxl/test/test_settings.h"

BT_DECLARE_FAKE_DRIVER();

using bt::LogSeverity;

namespace {

LogSeverity FxlLogToBtLogLevel(fxl::LogSeverity severity) {
  switch (severity) {
    case syslog::LOG_ERROR:
      return LogSeverity::ERROR;
    case syslog::LOG_WARNING:
      return LogSeverity::WARN;
    case syslog::LOG_INFO:
      return LogSeverity::INFO;
    case syslog::LOG_DEBUG:
      return LogSeverity::DEBUG;
    case syslog::LOG_TRACE:
      return LogSeverity::TRACE;
    default:
      break;
  }
  if (severity < 0) {
    return LogSeverity::TRACE;
  }
  return LogSeverity::ERROR;
}

}  // namespace

int main(int argc, char** argv) {
  auto cl = fxl::CommandLineFromArgcArgv(argc, argv);
  if (!fxl::SetTestSettings(cl)) {
    return EXIT_FAILURE;
  }

  fxl::LogSettings log_settings;
  log_settings.min_log_level = fxl::LOG_ERROR;
  if (!fxl::ParseLogSettings(cl, &log_settings)) {
    return EXIT_FAILURE;
  }

  // Set all library log messages to use printf instead ddk logging.
  bt::UsePrintf(FxlLogToBtLogLevel(log_settings.min_log_level));

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
