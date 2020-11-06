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

#include "pw_unit_test/unit_test_service.h"

#include "pw_log/log.h"
#include "pw_protobuf/decoder.h"
#include "pw_unit_test/framework.h"

namespace pw::unit_test {

void UnitTestService::Run(ServerContext&,
                          ConstByteSpan request,
                          RawServerWriter& writer) {
  writer_ = std::move(writer);
  verbose_ = false;

  protobuf::Decoder decoder(request);

  Status status;
  while ((status = decoder.Next()).ok()) {
    switch (static_cast<TestRunRequest::Fields>(decoder.FieldNumber())) {
      case TestRunRequest::Fields::REPORT_PASSED_EXPECTATIONS:
        decoder.ReadBool(&verbose_);
        break;
    }
  }

  if (status != Status::OutOfRange()) {
    writer_.Finish(status);
    return;
  }

  PW_LOG_DEBUG("Starting unit test run");

  pw::unit_test::RegisterEventHandler(&handler_);
  RUN_ALL_TESTS();
  pw::unit_test::RegisterEventHandler(nullptr);
  PW_LOG_DEBUG("Unit test run complete");

  writer_.Finish();
}

void UnitTestService::WriteTestRunStart() {
  // Write out the key for the start field (even though the message is empty).
  WriteEvent([&](Event::Encoder& event) { event.GetTestRunStartEncoder(); });
}

void UnitTestService::WriteTestRunEnd(const RunTestsSummary& summary) {
  WriteEvent([&](Event::Encoder& event) {
    TestRunEnd::Encoder test_run_end = event.GetTestRunEndEncoder();
    test_run_end.WritePassed(summary.passed_tests);
    test_run_end.WriteFailed(summary.failed_tests);
  });
}

void UnitTestService::WriteTestCaseStart(const TestCase& test_case) {
  WriteEvent([&](Event::Encoder& event) {
    TestCaseDescriptor::Encoder descriptor = event.GetTestCaseStartEncoder();
    descriptor.WriteSuiteName(test_case.suite_name);
    descriptor.WriteTestName(test_case.test_name);
    descriptor.WriteFileName(test_case.file_name);
  });
}

void UnitTestService::WriteTestCaseEnd(TestResult result) {
  WriteEvent([&](Event::Encoder& event) {
    event.WriteTestCaseEnd(static_cast<TestCaseResult>(result));
  });
}

void UnitTestService::WriteTestCaseDisabled(const TestCase& test_case) {
  WriteEvent([&](Event::Encoder& event) {
    TestCaseDescriptor::Encoder descriptor = event.GetTestCaseDisabledEncoder();
    descriptor.WriteSuiteName(test_case.suite_name);
    descriptor.WriteTestName(test_case.test_name);
    descriptor.WriteFileName(test_case.file_name);
  });
}

void UnitTestService::WriteTestCaseExpectation(
    const TestExpectation& expectation) {
  if (!verbose_ && expectation.success) {
    return;
  }

  WriteEvent([&](Event::Encoder& event) {
    TestCaseExpectation::Encoder test_case_expectation =
        event.GetTestCaseExpectationEncoder();
    test_case_expectation.WriteExpression(expectation.expression);
    test_case_expectation.WriteEvaluatedExpression(
        expectation.evaluated_expression);
    test_case_expectation.WriteLineNumber(expectation.line_number);
    test_case_expectation.WriteSuccess(expectation.success);
  });
}

}  // namespace pw::unit_test
