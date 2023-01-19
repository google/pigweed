// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include <gtest/gtest.h>
#include <pw_random/xor_shift.h>
#include <pw_string/format.h>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/lib/fxl/command_line.h"
#include "src/lib/fxl/log_settings_command_line.h"
#include "src/lib/fxl/test/test_settings.h"

#ifdef PW_LOG_DECLARE_FAKE_DRIVER
PW_LOG_DECLARE_FAKE_DRIVER();
#endif

using bt::LogSeverity;

namespace {

LogSeverity FxlLogToBtLogLevel(syslog::LogSeverity severity) {
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

// A valid random seed must be in [1, kMaxRandomSeed].
constexpr uint32_t kMaxRandomSeed = 99999;

// Normalizes the seed to range [1, kMaxRandomSeed].
int32_t NormalizeRandomSeed(uint32_t seed) {
  return static_cast<int32_t>((seed - 1U) % kMaxRandomSeed) + 1;
}

int32_t GenerateRandomSeed() {
  // TODO(fxbug.dev/118898): Get time using pw::chrono for portability.
  const int64_t time_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(0))
          .count();
  return NormalizeRandomSeed(static_cast<uint32_t>(time_ms));
}

}  // namespace

int main(int argc, char** argv) {
  fxl::CommandLine cl = fxl::CommandLineFromArgcArgv(argc, argv);
  if (!fxl::SetTestSettings(cl)) {
    return EXIT_FAILURE;
  }

  syslog::LogSettings log_settings;
  log_settings.min_log_level = syslog::LOG_ERROR;
  if (!fxl::ParseLogSettings(cl, &log_settings)) {
    return EXIT_FAILURE;
  }

  // Set all library log messages to use printf instead ddk logging.
  bt::UsePrintf(FxlLogToBtLogLevel(log_settings.min_log_level));

  // If --gtest_random_seed is not specified, then GoogleTest calculates a seed based on time. To
  // avoid using different seeds, we need to tell GoogleTest what seed we are using.
  std::vector<char*> new_argv(argv, argv + argc);
  char new_argv_seed_option[sizeof("--gtest_random_seed=-2147483648")] = {};

  // GoogleTest doesn't initialize the random seed (UnitTest::random_seed()) until RUN_ALL_TESTS, so
  // we need to parse it now to avoid configuring the random generator in every test suite.
  int32_t random_seed = 0;
  std::string seed_value;
  if (cl.GetOptionValue("gtest_random_seed", &seed_value)) {
    random_seed = stoi(seed_value);
    random_seed = NormalizeRandomSeed(random_seed);
  } else {
    random_seed = GenerateRandomSeed();
    BT_ASSERT(pw::string::Format(new_argv_seed_option, "--gtest_random_seed=%d", random_seed).ok());
    new_argv.push_back(new_argv_seed_option);
  }

  // Print the random seed so that it is easy to reproduce a test run.
  printf("\nGTEST_RANDOM_SEED=%d\n", random_seed);

  pw::random::XorShiftStarRng64 rng(random_seed);
  bt::set_random_generator(&rng);

  int new_argc = static_cast<int>(new_argv.size());
  // argv[argc] must be nullptr according to the C++ standard.
  new_argv.push_back(nullptr);

  testing::InitGoogleTest(&new_argc, new_argv.data());

  return RUN_ALL_TESTS();
}
