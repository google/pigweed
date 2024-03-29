// Copyright 2020 The Pigweed Authors
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

#include "pw_unit_test/event_handler.h"

namespace pw::unit_test {

/// Predefined event handler implementation that produces human-readable
/// GoogleTest-style test output and logs it via ``pw_log``. See
/// ``pw::unit_test::EventHandler`` for explanations of emitted events.
class LoggingEventHandler : public EventHandler {
 public:
  // If verbose is set, expectations values are always displayed.
  LoggingEventHandler(bool verbose = false) : verbose_(verbose) {}
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

 private:
  bool verbose_;
};

}  // namespace pw::unit_test
