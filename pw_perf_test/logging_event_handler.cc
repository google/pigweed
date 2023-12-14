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
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "pw_perf_test/logging_event_handler.h"

#include "pw_log/log.h"
#include "pw_perf_test/event_handler.h"
#include "pw_perf_test/googletest_style_event_handler.h"
#include "pw_perf_test/internal/timer.h"

namespace pw::perf_test {

void LoggingEventHandler::RunAllTestsStart(const TestRunInfo& summary) {
  PW_LOG_INFO(PW_PERF_TEST_GOOGLETEST_RUN_ALL_TESTS_START);
  PW_LOG_INFO(PW_PERF_TEST_GOOGLETEST_BEGINNING_SUMMARY,
              summary.total_tests,
              summary.default_iterations);
}

void LoggingEventHandler::RunAllTestsEnd() {
  PW_LOG_INFO(PW_PERF_TEST_GOOGLETEST_RUN_ALL_TESTS_END);
}

void LoggingEventHandler::TestCaseStart(const TestCase& info) {
  PW_LOG_INFO(PW_PERF_TEST_GOOGLETEST_CASE_START, info.name);
}

void LoggingEventHandler::TestCaseIteration(const TestIteration& iteration) {
  PW_LOG_DEBUG(PW_PERF_TEST_GOOGLETEST_CASE_ITERATION,
               static_cast<unsigned>(iteration.number),
               static_cast<unsigned long>(iteration.result),
               internal::GetDurationUnitStr());
}

void LoggingEventHandler::TestCaseMeasure(const TestMeasurement& measurement) {
  PW_LOG_INFO(PW_PERF_TEST_GOOGLETEST_CASE_MEASUREMENT,
              static_cast<unsigned long>(measurement.mean),
              internal::GetDurationUnitStr(),
              static_cast<unsigned long>(measurement.min),
              internal::GetDurationUnitStr(),
              static_cast<unsigned long>(measurement.max),
              internal::GetDurationUnitStr());
}

void LoggingEventHandler::TestCaseEnd(const TestCase& info) {
  PW_LOG_INFO(PW_PERF_TEST_GOOGLETEST_CASE_END, info.name);
}

}  // namespace pw::perf_test
