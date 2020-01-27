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

#pragma once

#include <cstddef>
#include <string_view>

#include "pw_preprocessor/compiler.h"
#include "pw_unit_test/event_handler.h"

namespace pw::unit_test {

// An event handler implementation which produces human-readable test output.
//
// Example output:
//
//   >>> Running MyTestSuite.TestCase1
//   [SUCCESS] 128 <= 129
//   [FAILURE] 'a' == 'b'
//     at ../path/to/my/file_test.cc:4831
//   <<< Test MyTestSuite.TestCase1 failed
//
class SimplePrintingEventHandler : public EventHandler {
 public:
  // Function for writing output as a string.
  using WriteFunction = void (*)(const std::string_view& string,
                                 bool append_newline);

  // Instantiates an event handler with a function to which to output results.
  // If verbose is set, information for successful tests is written as well as
  // failures.
  SimplePrintingEventHandler(WriteFunction write_function, bool verbose = false)
      : write_(write_function), verbose_(verbose) {}

  void RunAllTestsStart() override;
  void RunAllTestsEnd(const RunTestsSummary& run_tests_summary) override;
  void TestCaseStart(const TestCase& test_case) override;
  void TestCaseEnd(const TestCase& test_case, TestResult result) override;
  void TestCaseExpect(const TestCase& test_case,
                      const TestExpectation& expectation) override;
  void TestCaseDisabled(const TestCase& test_case) override;

 private:
  void WriteLine(const char* format, ...) PW_PRINTF_FORMAT(2, 3);

  WriteFunction write_;
  bool verbose_;
  char buffer_[512];
};

}  // namespace pw::unit_test
