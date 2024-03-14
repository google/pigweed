// Copyright 2024 The Pigweed Authors
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

#include <string_view>

#include "pw_json/builder.h"
#include "pw_unit_test/event_handler.h"
#include "pw_unit_test/internal/test_record_trie.h"

namespace pw::unit_test {
namespace json_impl {

inline constexpr const char* kSkipMacroIndicator = "(test skipped)";

}  // namespace json_impl

/// Predefined event handler implementation that outputs a test record (or
/// summary) in Chromium JSON Test Results Format. To use it, register event
/// handler, call the ``RUN_ALL_TESTS`` macro, then extract the test record json
/// as a string using the ``GetTestRecordJsonString()`` method. See
/// ``pw::unit_test::EventHandler`` for explanations of emitted events.
/// @see
/// https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/testing/json_test_results_format.md
/// @warning This event handler uses dynamic allocation
/// (`new`/`delete`/`std::string`) to generate the test record json.
class TestRecordEventHandler : public EventHandler {
 public:
  /// Constructor for the event handler. Have to seconds_since_epoch since
  /// calling std::time(nullptr) in pigweed is not supported.
  ///
  /// @param[in] seconds_since_epoch Seconds since epoch. Used in the test
  /// record as the seconds since epoch for the start of the test run.
  TestRecordEventHandler(int64_t seconds_since_epoch)
      : seconds_since_epoch_(seconds_since_epoch) {}

  /// Called when a test case completes. Record the test case result in the test
  /// record trie.
  ///
  /// @param[in] test_case Test case that ended.
  ///
  /// @param[in] result Result of the test case.
  void TestCaseEnd(const TestCase& test_case, TestResult result) override {
    test_record_trie_.AddTestResult(test_case, result);
  }

  /// Called after all tests are run. Save the run tests summary for later use.
  ///
  /// @param[in] summary A test run summary. Contains counts of each test result
  /// type.
  void RunAllTestsEnd(const RunTestsSummary& summary) override {
    run_tests_summary_ = summary;
  }

  /// Called after each expect/assert statement within a test case with the
  /// result of the expectation.
  ///
  /// We usually expect all tests to PASS. However, if the
  /// GTEST_SKIP macro is used, the test is expected to be skipped and the
  /// expectation expression is replaced with "(test skipped)"
  ///
  /// @param[in] test_case Test case that the expect statement lies in.
  ///
  /// @param[in] expectation Current expectation being checked for the test
  /// case.
  void TestCaseExpect(const TestCase& test_case,
                      const TestExpectation& expectation) override {
    // TODO: b/329688428 - Check for test skips directly rather than doing a
    // string comparison
    if (std::string_view(json_impl::kSkipMacroIndicator) ==
        expectation.expression) {
      test_record_trie_.AddTestResultExpectation(test_case,
                                                 TestResult::kSkipped);
    }
  }

  /// Converts the test record trie into a json string and returns it.
  ///
  /// @param[in] max_json_buffer_size The max size (in bytes) of the buffer to
  /// allocate for the json string.
  ///
  /// @returns The test record json as a string.
  std::string GetTestRecordJsonString(size_t max_json_buffer_size) {
    return test_record_trie_.GetTestRecordJsonString(
        run_tests_summary_, seconds_since_epoch_, max_json_buffer_size);
  }

  void RunAllTestsStart() override {}
  void TestProgramStart(const ProgramSummary&) override {}
  void EnvironmentsSetUpEnd() override {}
  void TestSuiteStart(const TestSuite&) override {}
  void TestSuiteEnd(const TestSuite&) override {}
  void EnvironmentsTearDownEnd() override {}
  void TestProgramEnd(const ProgramSummary&) override {}
  void TestCaseStart(const TestCase&) override {}
  void TestCaseDisabled(const TestCase&) override {}

 private:
  // Seconds since epoch from the start of the test run.
  int64_t seconds_since_epoch_;

  // A summary of the test run. Set once RunAllTestsEnd is called and used when
  // the consumer of this event handler wants to generate the test record json
  // string.
  RunTestsSummary run_tests_summary_;

  // The entrypoint for interacting with the test record trie.
  json_impl::TestRecordTrie test_record_trie_;
};

}  // namespace pw::unit_test
