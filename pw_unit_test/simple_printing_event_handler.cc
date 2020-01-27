// Copyright 2019 The Pigweed Authors
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

#include "pw_unit_test/simple_printing_event_handler.h"

#include <cstdarg>
#include <cstdio>
#include <string_view>

namespace pw::unit_test {

void SimplePrintingEventHandler::RunAllTestsStart() {
  WriteLine("[==========] Running all tests.");
}

void SimplePrintingEventHandler::RunAllTestsEnd(
    const RunTestsSummary& run_tests_summary) {
  WriteLine("[==========] Done running all tests.");
  WriteLine("[  PASSED  ] %d test(s).", run_tests_summary.passed_tests);
  if (run_tests_summary.failed_tests) {
    WriteLine("[  FAILED  ] %d test(s).", run_tests_summary.failed_tests);
  }
}

void SimplePrintingEventHandler::TestCaseStart(const TestCase& test_case) {
  WriteLine("[ RUN      ] %s.%s", test_case.suite_name, test_case.test_name);
}

void SimplePrintingEventHandler::TestCaseEnd(const TestCase& test_case,
                                             TestResult result) {
  // Use a switch with no default to detect changes in the test result enum.
  switch (result) {
    case TestResult::kSuccess:
      WriteLine(
          "[       OK ] %s.%s", test_case.suite_name, test_case.test_name);
      break;
    case TestResult::kFailure:
      WriteLine(
          "[  FAILED  ] %s.%s", test_case.suite_name, test_case.test_name);
      break;
  }
}

void SimplePrintingEventHandler::TestCaseExpect(
    const TestCase& test_case, const TestExpectation& expectation) {
  if (!verbose_ && expectation.success) {
    return;
  }

  const char* result = expectation.success ? "Success" : "Failure";
  WriteLine("%s:%d: %s", test_case.file_name, expectation.line_number, result);
  WriteLine("      Expected: %s", expectation.expression);

  write_("        Actual: ", false);
  write_(expectation.evaluated_expression, true);
}

void SimplePrintingEventHandler::WriteLine(const char* format, ...) {
  va_list args;

  va_start(args, format);
  std::vsnprintf(buffer_, sizeof(buffer_), format, args);
  va_end(args);

  write_(buffer_, true);
}

void SimplePrintingEventHandler::TestCaseDisabled(const TestCase& test) {
  if (verbose_) {
    WriteLine("Skipping disabled test %s.%s", test.suite_name, test.test_name);
  }
}

}  // namespace pw::unit_test
