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

#pragma once

#include "pw_preprocessor/compiler.h"
#include "pw_unit_test/event_handler.h"

// Define the test messages and string formats as literal strings so they
// work with different log databases.
#define PW_UNIT_TEST_GOOGLETEST_RUN_ALL_TESTS_START \
  "[==========] Running all tests."
#define PW_UNIT_TEST_GOOGLETEST_RUN_ALL_TESTS_END \
  "[==========] Done running all tests."

#define PW_UNIT_TEST_GOOGLETEST_TEST_PROGRAM_START \
  "[==========] Running %d tests from %d test suite%s."

#define PW_UNIT_TEST_GOOGLETEST_TEST_PROGRAM_END \
  "[==========] %d / %d tests from %d test suite%s ran."

#define PW_UNIT_TEST_GOOGLETEST_ENVIRONMENTS_SETUP_END \
  "[----------] Global test environments setup."

#define PW_UNIT_TEST_GOOGLETEST_ENVIRONMENTS_TEAR_DOWN_END \
  "[----------] Global test environments tear-down."

#define PW_UNIT_TEST_GOOGLETEST_TEST_SUITE_START \
  "[----------] %d tests from %s."

#define PW_UNIT_TEST_GOOGLETEST_TEST_SUITE_END "[----------] %d tests from %s."

#define PW_UNIT_TEST_GOOGLETEST_PASSED_SUMMARY "[  PASSED  ] %d test(s)."
#define PW_UNIT_TEST_GOOGLETEST_DISABLED_SUMMARY "[ DISABLED ] %d test(s)."
#define PW_UNIT_TEST_GOOGLETEST_FAILED_SUMMARY "[  FAILED  ] %d test(s)."

#define PW_UNIT_TEST_GOOGLETEST_CASE_START "[ RUN      ] %s.%s"
#define PW_UNIT_TEST_GOOGLETEST_CASE_OK "[       OK ] %s.%s"
#define PW_UNIT_TEST_GOOGLETEST_CASE_FAILED "[  FAILED  ] %s.%s"
#define PW_UNIT_TEST_GOOGLETEST_CASE_DISABLED "[ DISABLED ] %s.%s"

namespace pw {
namespace unit_test {

// Renders the test results in Google Test style.
class GoogleTestStyleEventHandler : public EventHandler {
 public:
  void TestProgramStart(const ProgramSummary& program_summary) override;
  void EnvironmentsSetUpEnd() override;
  void TestSuiteStart(const TestSuite& test_suite) override;
  void TestSuiteEnd(const TestSuite& test_suite) override;
  void EnvironmentsTearDownEnd() override;
  void TestProgramEnd(const ProgramSummary& program_summary) override;

  void RunAllTestsStart() override;
  void RunAllTestsEnd(const RunTestsSummary& run_tests_summary) override;
  void TestCaseStart(const TestCase& test_case) override;
  void TestCaseEnd(const TestCase& test_case, TestResult result) override;
  void TestCaseExpect(const TestCase& test_case,
                      const TestExpectation& expectation) override;
  void TestCaseDisabled(const TestCase& test_case) override;

 protected:
  constexpr GoogleTestStyleEventHandler(bool verbose) : verbose_(verbose) {}

  bool verbose() const { return verbose_; }

  // Writes the content without a trailing newline.
  virtual void Write(const char* content) = 0;

  // Writes the formatted content and appends a newline character.
  virtual void WriteLine(const char* format, ...) PW_PRINTF_FORMAT(2, 3) = 0;

 private:
  bool verbose_;
};

}  // namespace unit_test
}  // namespace pw
