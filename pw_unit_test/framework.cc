// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#include "pw_unit_test/framework.h"

namespace pw::unit_test {

void RegisterEventHandler(EventHandler* event_handler) {
  internal::Framework::Get().RegisterEventHandler(event_handler);
}

namespace internal {

// Singleton instance of the unit test framework class.
Framework Framework::framework_;

// Linked list of all test cases in the test executable. This is static as it is
// populated using static initialization.
TestInfo* Framework::tests_ = nullptr;

void Framework::RegisterTest(TestInfo* test) {
  // Append the test case to the end of the test list.
  TestInfo** pos = &tests_;
  for (; *pos != nullptr; pos = &(*pos)->next) {
  }
  *pos = test;
}

int Framework::RunAllTests() {
  run_tests_summary_.passed_tests = 0;
  run_tests_summary_.failed_tests = 0;

  if (event_handler_ != nullptr) {
    event_handler_->RunAllTestsStart();
  }
  for (TestInfo* test = tests_; test != nullptr; test = test->next) {
    test->run();
  }
  if (event_handler_ != nullptr) {
    event_handler_->RunAllTestsEnd(run_tests_summary_);
  }
  return exit_status_;
}

void Framework::StartTest(Test* test) {
  current_test_ = test;
  current_result_ = TestResult::kSuccess;

  if (event_handler_ == nullptr) {
    return;
  }

  const TestInfo* info = test->pigweed_test_info_;
  TestCase test_case = {
      .suite_name = info->test_suite_name,
      .test_name = info->test_name,
      .file_name = info->file_name,
  };

  event_handler_->TestCaseStart(test_case);
}

void Framework::EndTest(Test* test) {
  current_test_ = nullptr;
  switch (current_result_) {
    case TestResult::kSuccess:
      run_tests_summary_.passed_tests++;
      break;
    case TestResult::kFailure:
      run_tests_summary_.failed_tests++;
      break;
  }

  if (event_handler_ == nullptr) {
    return;
  }

  const TestInfo* info = test->pigweed_test_info_;
  TestCase test_case = {
      .suite_name = info->test_suite_name,
      .test_name = info->test_name,
      .file_name = info->file_name,
  };

  event_handler_->TestCaseEnd(test_case, current_result_);
}

void Framework::ExpectationResult(const char* expression,
                                  int line,
                                  bool success) {
  if (!success) {
    current_result_ = TestResult::kFailure;
    exit_status_ = 1;
  }

  if (event_handler_ == nullptr) {
    return;
  }

  const TestInfo* info = current_test_->pigweed_test_info_;
  TestCase test_case = {
      .suite_name = info->test_suite_name,
      .test_name = info->test_name,
      .file_name = info->file_name,
  };

  TestExpectation expectation = {
      .expression = expression,
      .line_number = line,
      .success = success,
  };

  event_handler_->TestCaseExpect(test_case, expectation);
}

}  // namespace internal

}  // namespace pw::unit_test
