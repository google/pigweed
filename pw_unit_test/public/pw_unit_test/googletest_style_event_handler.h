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

namespace pw {
namespace unit_test {

// Renders the test results in Google Test style.
class GoogleTestStyleEventHandler : public EventHandler {
 public:
  static constexpr const char kRunAllTestsStart[] =
      "[==========] Running all tests.";
  static constexpr const char kRunAllTestsEnd[] =
      "[==========] Done running all tests.";

  static constexpr const char kPassedSummary[] = "[  PASSED  ] %d test(s).";
  static constexpr const char kSkippedSummary[] = "[  SKIPPED ] %d test(s).";
  static constexpr const char kFailedSummary[] = "[  FAILED  ] %d test(s).";

  static constexpr const char kCaseStart[] = "[ RUN      ] %s.%s";
  static constexpr const char kCaseOk[] = "[       OK ] %s.%s";
  static constexpr const char kCaseFailed[] = "[  FAILED  ] %s.%s";
  static constexpr const char kCaseSkipped[] = "[  SKIPPED ] %s.%s";

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
