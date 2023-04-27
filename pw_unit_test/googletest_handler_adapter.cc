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

#include "pw_unit_test/googletest_handler_adapter.h"

#include <cstdlib>

namespace pw::unit_test {

void RegisterEventHandler(EventHandler* event_handler) {
  static testing::TestEventListener* gTestListener = nullptr;
  auto& listeners = testing::UnitTest::GetInstance()->listeners();
  if (!gTestListener) {
    gTestListener = listeners.default_result_printer();
  }
  if (gTestListener) {
    listeners.Release(gTestListener);
    delete gTestListener;
  }
  if (event_handler) {
    gTestListener = new pw::unit_test::GoogleTestHandlerAdapter(*event_handler);
    listeners.Append(gTestListener);
  }
}

void GoogleTestHandlerAdapter::OnTestProgramStart(
    const testing::UnitTest& unit_test) {
  handler_.TestProgramStart(
      {unit_test.test_to_run_count(), unit_test.test_suite_to_run_count(), {}});
}

void GoogleTestHandlerAdapter::OnEnvironmentsSetUpEnd(
    const ::testing::UnitTest&) {
  handler_.EnvironmentsSetUpEnd();
}

void GoogleTestHandlerAdapter::OnTestSuiteStart(
    const ::testing::TestSuite& ts) {
  handler_.TestSuiteStart({ts.name(), ts.test_to_run_count()});
}

void GoogleTestHandlerAdapter::OnTestStart(const ::testing::TestInfo& ti) {
  handler_.TestCaseStart({ti.test_suite_name(), ti.name(), ti.file()});
}

void GoogleTestHandlerAdapter::OnTestPartResult(
    const ::testing::TestPartResult& tpr) {
  handler_.TestCaseExpect(
      {.suite_name = "", .test_name = "", .file_name = tpr.file_name()},
      {.expression = "",
       .evaluated_expression = tpr.summary(),
       .line_number = tpr.line_number(),
       .success = tpr.passed() || tpr.skipped()});
}

void GoogleTestHandlerAdapter::OnTestEnd(const ::testing::TestInfo& ti) {
  auto result = ti.result()->Passed()
                    ? TestResult::kSuccess
                    : (ti.result()->Failed() ? TestResult::kFailure
                                             : TestResult::kSkipped);

  handler_.TestCaseEnd({ti.test_suite_name(), ti.name(), ti.file()}, result);
}

void GoogleTestHandlerAdapter::OnTestSuiteEnd(const ::testing::TestSuite& ts) {
  handler_.TestSuiteEnd({ts.name(), ts.test_to_run_count()});
}

void GoogleTestHandlerAdapter::OnEnvironmentsTearDownEnd(
    const ::testing::UnitTest&) {
  handler_.EnvironmentsTearDownEnd();
}

void GoogleTestHandlerAdapter::OnTestProgramEnd(
    const ::testing::UnitTest& unit_test) {
  handler_.TestProgramEnd({unit_test.test_to_run_count(),
                           unit_test.test_suite_to_run_count(),
                           {unit_test.successful_test_count(),
                            unit_test.failed_test_count(),
                            unit_test.skipped_test_count(),
                            unit_test.disabled_test_count()}});
}

}  // namespace pw::unit_test
