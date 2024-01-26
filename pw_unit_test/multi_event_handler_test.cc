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

#include "pw_unit_test/multi_event_handler.h"

#include "pw_unit_test/framework.h"

namespace pw::unit_test {
namespace {

// Fake event handler that keeps track of how many times its functions were
// invoked
class FakeEventHandler : public EventHandler {
 public:
  struct {
    int TestProgramStart = 0;
    int EnvironmentsSetUpEnd = 0;
    int TestSuiteStart = 0;
    int TestSuiteEnd = 0;
    int EnvironmentsTearDownEnd = 0;
    int TestProgramEnd = 0;
    int RunAllTestsStart = 0;
    int RunAllTestsEnd = 0;
    int TestCaseStart = 0;
    int TestCaseEnd = 0;
    int TestCaseExpect = 0;
    int TestCaseDisabled = 0;
  } function_invocation_counts;

  void TestProgramStart(const ProgramSummary&) override {
    function_invocation_counts.TestProgramStart++;
  }
  void EnvironmentsSetUpEnd() override {
    function_invocation_counts.EnvironmentsSetUpEnd++;
  }
  void TestSuiteStart(const TestSuite&) override {
    function_invocation_counts.TestSuiteStart++;
  }
  void TestSuiteEnd(const TestSuite&) override {
    function_invocation_counts.TestSuiteEnd++;
  }
  void EnvironmentsTearDownEnd() override {
    function_invocation_counts.EnvironmentsTearDownEnd++;
  }
  void TestProgramEnd(const ProgramSummary&) override {
    function_invocation_counts.TestProgramEnd++;
  }
  void RunAllTestsStart() override {
    function_invocation_counts.RunAllTestsStart++;
  }
  void RunAllTestsEnd(const RunTestsSummary&) override {
    function_invocation_counts.RunAllTestsEnd++;
  }
  void TestCaseStart(const TestCase&) override {
    function_invocation_counts.TestCaseStart++;
  }
  void TestCaseEnd(const TestCase&, TestResult) override {
    function_invocation_counts.TestCaseEnd++;
  }
  void TestCaseExpect(const TestCase&, const TestExpectation&) override {
    function_invocation_counts.TestCaseExpect++;
  }
  void TestCaseDisabled(const TestCase&) override {
    function_invocation_counts.TestCaseDisabled++;
  }
};

// Helper method for ensuring all methods of an event handler were called x
// number of times
void AssertFunctionInvocationCounts(FakeEventHandler handler,
                                    int num_invocations) {
  ASSERT_EQ(handler.function_invocation_counts.TestProgramStart,
            num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.EnvironmentsSetUpEnd,
            num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.TestSuiteStart, num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.TestSuiteEnd, num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.EnvironmentsTearDownEnd,
            num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.TestProgramEnd, num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.RunAllTestsStart,
            num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.RunAllTestsEnd, num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.TestCaseStart, num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.TestCaseEnd, num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.TestCaseExpect, num_invocations);
  ASSERT_EQ(handler.function_invocation_counts.TestCaseDisabled,
            num_invocations);
}

TEST(AllEventHandlerMethodsCalled, InvokeMethodMultipleTimes) {
  FakeEventHandler h1;
  FakeEventHandler h2;
  MultiEventHandler<2> multi_handler(h1, h2);

  ASSERT_EQ(h1.function_invocation_counts.TestCaseStart, 0);
  ASSERT_EQ(h2.function_invocation_counts.TestCaseEnd, 0);

  TestCase test_case{};
  TestResult test_result = TestResult::kSuccess;
  multi_handler.TestCaseStart(test_case);
  multi_handler.TestCaseStart(test_case);
  multi_handler.TestCaseStart(test_case);
  multi_handler.TestCaseEnd(test_case, test_result);
  multi_handler.TestCaseEnd(test_case, test_result);
  multi_handler.TestCaseEnd(test_case, test_result);

  ASSERT_EQ(h1.function_invocation_counts.TestCaseStart, 3);
  ASSERT_EQ(h2.function_invocation_counts.TestCaseEnd, 3);
}

TEST(AllEventHandlerMethodsCalled, InvokeAllEventHandlerMethods) {
  FakeEventHandler h1;
  FakeEventHandler h2;
  MultiEventHandler<2> multi_handler(h1, h2);

  AssertFunctionInvocationCounts(h1, 0);
  AssertFunctionInvocationCounts(h2, 0);

  ProgramSummary program_summary{};
  TestSuite test_suite{};
  TestCase test_case{};
  RunTestsSummary run_test_summary{};
  TestExpectation expectation{};
  TestResult test_result = TestResult::kSuccess;
  multi_handler.TestProgramStart(program_summary);
  multi_handler.EnvironmentsSetUpEnd();
  multi_handler.TestSuiteStart(test_suite);
  multi_handler.TestSuiteEnd(test_suite);
  multi_handler.EnvironmentsTearDownEnd();
  multi_handler.TestProgramEnd(program_summary);
  multi_handler.RunAllTestsStart();
  multi_handler.RunAllTestsEnd(run_test_summary);
  multi_handler.TestCaseStart(test_case);
  multi_handler.TestCaseEnd(test_case, test_result);
  multi_handler.TestCaseExpect(test_case, expectation);
  multi_handler.TestCaseDisabled(test_case);

  AssertFunctionInvocationCounts(h1, 1);
  AssertFunctionInvocationCounts(h2, 1);
}

}  // namespace
}  // namespace pw::unit_test