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

#include "pw_perf_test/internal/framework.h"

#include "pw_perf_test/internal/test_info.h"
#include "pw_perf_test/internal/timer.h"

namespace pw::perf_test::internal {

Framework Framework::framework_;

int Framework::RunAllTests() {
  if (!internal::TimerPrepare()) {
    return false;
  }

  event_handler_->RunAllTestsStart(run_info_);

  for (const TestInfo* test = tests_; test != nullptr; test = test->next()) {
    State test_state = internal::CreateState(
        kDefaultIterations, *event_handler_, test->test_name());
    test->Run(test_state);
  }
  internal::TimerCleanup();
  event_handler_->RunAllTestsEnd();
  return true;
}

void Framework::RegisterTest(TestInfo& new_test) {
  ++run_info_.total_tests;
  if (tests_ == nullptr) {
    tests_ = &new_test;
    return;
  }
  TestInfo* info = tests_;
  for (; info->next() != nullptr; info = info->next()) {
  }
  info->SetNext(&new_test);
}

}  // namespace pw::perf_test::internal
