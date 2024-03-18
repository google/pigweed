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

#include "pw_unit_test/test_record_event_handler.h"

#include "pw_assert/assert.h"
#include "pw_unit_test/framework.h"

namespace pw::unit_test {
namespace {

time_t kFakeTimeSinceEpoch = 12345;

TEST(TestRecordFunctionality, SingleTestRun) {
  TestRecordEventHandler test_record_event_handler(kFakeTimeSinceEpoch);

  // Initialize test cases to be run
  TestCase test_case{"suite1", "test1", "dir1/test_file.cc"};
  TestResult test_result = TestResult::kSuccess;
  RunTestsSummary run_tests_summary = {1, 0, 0, 0};

  // Simulate test run
  test_record_event_handler.TestCaseStart(test_case);
  test_record_event_handler.TestCaseEnd(test_case, test_result);
  test_record_event_handler.RunAllTestsEnd(run_tests_summary);

  std::string actual = test_record_event_handler.GetTestRecordJsonString(300);
  std::string expected =
      R"({"tests": {"dir1": {"test_file.cc": {"suite1":)"
      R"( {"test1": {"expected": "PASS", "actual": "PASS"}}}}},)"
      R"( "version": 3, "interrupted": false, "seconds_since_epoch": 12345,)"
      R"( "num_failures_by_type": {"PASS": 1, "FAIL": 0, "SKIP": 0}})";
  ASSERT_EQ(actual, expected);
}

TEST(TestRecordFunctionality, MultipleTestsRun) {
  TestRecordEventHandler test_record_event_handler(kFakeTimeSinceEpoch);

  // Initialize test cases to be run
  TestCase test_case1{"suite1", "test1", "dir1/test_file.cc"};
  TestResult test_result1 = TestResult::kSuccess;
  TestCase test_case2{"suite2", "test2", "dir1/test_file.cc"};
  TestResult test_result2 = TestResult::kFailure;
  TestCase test_case3{"suite3", "test3", "dir1/dir2/test_file.cc"};
  TestResult test_result3 = TestResult::kSuccess;
  RunTestsSummary run_tests_summary = {2, 1, 0, 0};

  // Simulate test run
  test_record_event_handler.TestCaseStart(test_case1);
  test_record_event_handler.TestCaseEnd(test_case1, test_result1);
  test_record_event_handler.TestCaseStart(test_case2);
  test_record_event_handler.TestCaseEnd(test_case2, test_result2);
  test_record_event_handler.TestCaseStart(test_case3);
  test_record_event_handler.TestCaseEnd(test_case3, test_result3);
  test_record_event_handler.RunAllTestsEnd(run_tests_summary);

  std::string actual = test_record_event_handler.GetTestRecordJsonString(400);

  std::string expected =
      R"({"tests": {"dir1": {"dir2": {"test_file.cc": {"suite3": {"test3":)"
      R"( {"expected": "PASS", "actual": "PASS"}}}}, "test_file.cc":)"
      R"( {"suite2": {"test2": {"expected": "PASS", "actual": "FAIL"}},)"
      R"( "suite1": {"test1": {"expected": "PASS", "actual": "PASS"}}}}},)"
      R"( "version": 3, "interrupted": false, "seconds_since_epoch": 12345,)"
      R"( "num_failures_by_type": {"PASS": 2, "FAIL": 1, "SKIP": 0}})";
  ASSERT_EQ(actual, expected);
}

TEST(TestRecordFunctionality, JsonBufferTooSmall) {
  TestRecordEventHandler test_record_event_handler(kFakeTimeSinceEpoch);

  // Initialize test cases to be run
  TestCase test_case{"suite1", "test1", "dir1/test_file.cc"};
  TestResult test_result = TestResult::kSuccess;
  RunTestsSummary run_tests_summary = {1, 0, 0, 0};

  // Simulate test run
  test_record_event_handler.TestCaseStart(test_case);
  test_record_event_handler.TestCaseEnd(test_case, test_result);
  test_record_event_handler.RunAllTestsEnd(run_tests_summary);
  EXPECT_DEATH_IF_SUPPORTED(
      test_record_event_handler.GetTestRecordJsonString(10),
      "Test record json buffer is not big enough, please increase size.");
}

TEST(TestRecordFunctionality, HandleSkipMacro) {
  TestRecordEventHandler test_record_event_handler(kFakeTimeSinceEpoch);

  // Initialize test cases to be run
  TestCase test_case{"suite1", "test1", "dir1/test_file.cc"};
  TestResult test_result = TestResult::kSkipped;
  TestExpectation skip_expectation = {
      pw::unit_test::json_impl::kSkipMacroIndicator,
      pw::unit_test::json_impl::kSkipMacroIndicator,
      0,
      true};
  RunTestsSummary run_tests_summary = {0, 0, 1, 0};

  // Simulate test run
  test_record_event_handler.TestCaseStart(test_case);
  test_record_event_handler.TestCaseExpect(test_case, skip_expectation);
  test_record_event_handler.TestCaseEnd(test_case, test_result);
  test_record_event_handler.RunAllTestsEnd(run_tests_summary);

  std::string actual = test_record_event_handler.GetTestRecordJsonString(300);
  std::string expected =
      R"({"tests": {"dir1": {"test_file.cc": {"suite1":)"
      R"( {"test1": {"expected": "SKIP", "actual": "SKIP"}}}}},)"
      R"( "version": 3, "interrupted": false, "seconds_since_epoch": 12345,)"
      R"( "num_failures_by_type": {"PASS": 0, "FAIL": 0, "SKIP": 1}})";
  ASSERT_EQ(actual, expected);
}

TEST(TestRecordFunctionality, DuplicateTest) {
  TestRecordEventHandler test_record_event_handler(kFakeTimeSinceEpoch);

  // Initialize test cases to be run
  TestCase test_case1{"suite1", "test1", "dir1/test_file.cc"};
  TestResult test_result1 = TestResult::kSuccess;
  TestCase test_case2{"suite1", "test1", "dir1/test_file.cc"};
  TestResult test_result2 = TestResult::kFailure;
  RunTestsSummary run_tests_summary = {0, 1, 0, 0};

  // Simulate test run
  test_record_event_handler.TestCaseStart(test_case1);
  test_record_event_handler.TestCaseEnd(test_case1, test_result1);
  test_record_event_handler.TestCaseStart(test_case2);
  test_record_event_handler.TestCaseEnd(test_case2, test_result2);
  test_record_event_handler.RunAllTestsEnd(run_tests_summary);

  std::string actual = test_record_event_handler.GetTestRecordJsonString(300);

  std::string expected =
      R"({"tests": {"dir1": {"test_file.cc": {"suite1":)"
      R"( {"test1": {"expected": "PASS", "actual": "FAIL"}}}}},)"
      R"( "version": 3, "interrupted": false, "seconds_since_epoch": 12345,)"
      R"( "num_failures_by_type": {"PASS": 0, "FAIL": 1, "SKIP": 0}})";
  ASSERT_EQ(actual, expected);
}

}  // namespace
}  // namespace pw::unit_test