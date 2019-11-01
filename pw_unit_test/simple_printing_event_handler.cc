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

#include "pw_unit_test/simple_printing_event_handler.h"

#include <cstdarg>
#include <cstdio>

namespace pw::unit_test {

void SimplePrintingEventHandler::TestCaseStart(const TestCase& test_case) {
  WriteAndFlush(
      ">>> Running %s.%s\n", test_case.suite_name, test_case.test_name);
}

void SimplePrintingEventHandler::TestCaseEnd(const TestCase& test_case,
                                             TestResult result) {
  const char* status = result == TestResult::kSuccess ? "succeeded" : "failed";
  WriteAndFlush(
      "<<< Test %s.%s %s\n", test_case.suite_name, test_case.test_name, status);
}

void SimplePrintingEventHandler::TestCaseExpect(
    const TestCase& test_case, const TestExpectation& expectation) {
  if (!verbose_ && expectation.success) {
    return;
  }

  const char* result = expectation.success ? "SUCCESS" : "FAILURE";
  WriteAndFlush("[%s] %s\n", result, expectation.expression);
  WriteAndFlush("  at %s:%d\n", test_case.file_name, expectation.line_number);
}

int SimplePrintingEventHandler::WriteAndFlush(const char* format, ...) {
  va_list args;

  va_start(args, format);
  std::vsnprintf(buffer_, sizeof(buffer_), format, args);
  va_end(args);

  return write_(buffer_);
}

}  // namespace pw::unit_test
