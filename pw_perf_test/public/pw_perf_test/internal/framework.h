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

#include "pw_perf_test/event_handler.h"

namespace pw::perf_test::internal {

// Forward declaration.
class TestInfo;

/// Singleton that manages and runs performance tests.
///
/// This class mimics pw::unit_test::Framework.
class Framework {
 public:
  constexpr Framework()
      : event_handler_(nullptr),
        tests_(nullptr),
        run_info_{.total_tests = 0, .default_iterations = kDefaultIterations} {}

  static Framework& Get() { return framework_; }

  void RegisterEventHandler(EventHandler& event_handler) {
    event_handler_ = &event_handler;
  }

  void RegisterTest(TestInfo&);

  int RunAllTests();

 private:
  static constexpr int kDefaultIterations = 10;

  EventHandler* event_handler_;

  // Pointer to the list of tests
  TestInfo* tests_;

  TestRunInfo run_info_;

  // Singleton
  static Framework framework_;
};

}  // namespace pw::perf_test::internal
