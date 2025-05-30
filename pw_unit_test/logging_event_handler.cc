// Copyright 2022 The Pigweed Authors
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

#include "pw_unit_test/logging_event_handler.h"

#include <cstdint>

#include "pw_log/log.h"
#include "pw_unit_test/googletest_style_event_handler.h"

namespace pw::unit_test {

void LoggingEventHandler::TestProgramStart(
    const ProgramSummary& program_summary) {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_TEST_PROGRAM_START,
              program_summary.tests_to_run,
              program_summary.test_suites,
              program_summary.test_suites != 1 ? "s" : "");
}

void LoggingEventHandler::EnvironmentsSetUpEnd() {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_ENVIRONMENTS_SETUP_END);
}

void LoggingEventHandler::TestSuiteStart(const TestSuite& test_suite) {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_TEST_SUITE_START,
              test_suite.test_to_run_count,
              test_suite.name);
}

void LoggingEventHandler::TestSuiteEnd(const TestSuite& test_suite) {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_TEST_SUITE_END,
              test_suite.test_to_run_count,
              test_suite.name);
}

void LoggingEventHandler::EnvironmentsTearDownEnd() {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_ENVIRONMENTS_TEAR_DOWN_END);
}

void LoggingEventHandler::TestProgramEnd(
    const ProgramSummary& program_summary) {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_TEST_PROGRAM_END,
              program_summary.tests_to_run -
                  program_summary.tests_summary.skipped_tests -
                  program_summary.tests_summary.disabled_tests,
              program_summary.tests_to_run,
              program_summary.test_suites,
              program_summary.test_suites != 1 ? "s" : "");
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_PASSED_SUMMARY,
              program_summary.tests_summary.passed_tests);
  if (program_summary.tests_summary.skipped_tests ||
      program_summary.tests_summary.disabled_tests) {
    PW_LOG_WARN(PW_UNIT_TEST_GOOGLETEST_DISABLED_SUMMARY,
                program_summary.tests_summary.skipped_tests +
                    program_summary.tests_summary.disabled_tests);
  }
  if (program_summary.tests_summary.failed_tests) {
    PW_LOG_ERROR(PW_UNIT_TEST_GOOGLETEST_FAILED_SUMMARY,
                 program_summary.tests_summary.failed_tests);
  }
}

void LoggingEventHandler::RunAllTestsStart() {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_RUN_ALL_TESTS_START);
}

void LoggingEventHandler::RunAllTestsEnd(
    const RunTestsSummary& run_tests_summary) {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_RUN_ALL_TESTS_END);
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_PASSED_SUMMARY,
              run_tests_summary.passed_tests);
  if (run_tests_summary.skipped_tests) {
    PW_LOG_WARN(PW_UNIT_TEST_GOOGLETEST_DISABLED_SUMMARY,
                run_tests_summary.skipped_tests);
  }
  if (run_tests_summary.failed_tests) {
    PW_LOG_ERROR(PW_UNIT_TEST_GOOGLETEST_FAILED_SUMMARY,
                 run_tests_summary.failed_tests);
  }
}

void LoggingEventHandler::TestCaseStart(const TestCase& test_case) {
  PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_CASE_START,
              test_case.suite_name,
              test_case.test_name);
}

void LoggingEventHandler::TestCaseEnd(const TestCase& test_case,
                                      TestResult result) {
  // Use a switch with no default to detect changes in the test result enum.
  switch (result) {
    case TestResult::kSuccess:
      PW_LOG_INFO(PW_UNIT_TEST_GOOGLETEST_CASE_OK,
                  test_case.suite_name,
                  test_case.test_name);
      break;
    case TestResult::kFailure:
      PW_LOG_ERROR(PW_UNIT_TEST_GOOGLETEST_CASE_FAILED,
                   test_case.suite_name,
                   test_case.test_name);
      break;
    case TestResult::kSkipped:
      PW_LOG_WARN(PW_UNIT_TEST_GOOGLETEST_CASE_DISABLED,
                  test_case.suite_name,
                  test_case.test_name);
      break;
  }
}

void LoggingEventHandler::TestCaseExpect(const TestCase& test_case,
                                         const TestExpectation& expectation) {
  if (!verbose_ && expectation.success) {
    return;
  }

  const char* result = expectation.success ? "Success" : "Failure";
  int32_t level = expectation.success ? PW_LOG_LEVEL_INFO : PW_LOG_LEVEL_ERROR;
  PW_LOG(level,
         PW_LOG_LEVEL,
         PW_LOG_MODULE_NAME,
         PW_LOG_FLAGS,
         "%s:%d: %s",
         test_case.file_name,
         expectation.line_number,
         result);
  PW_LOG(level,
         PW_LOG_LEVEL,
         PW_LOG_MODULE_NAME,
         PW_LOG_FLAGS,
         "      Expected: %s",
         expectation.expression);
  PW_LOG(level,
         PW_LOG_LEVEL,
         PW_LOG_MODULE_NAME,
         PW_LOG_FLAGS,
         "        Actual: %s",
         expectation.evaluated_expression);
}

void LoggingEventHandler::TestCaseDisabled(const TestCase& test) {
  PW_LOG_DEBUG("Skipping disabled test %s.%s", test.suite_name, test.test_name);
}

}  // namespace pw::unit_test
