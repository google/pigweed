// Copyright 2025 The Pigweed Authors
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

#include "pw_perf_test/log_csv_event_handler.h"

#include "pw_log/log.h"
#include "pw_perf_test/internal/timer.h"

namespace pw::perf_test {

void LogCsvEventHandler::RunAllTestsStart(const TestRunInfo&) {
  PW_LOG_INFO("test name,total iterations,min,max,mean,unit");
}

void LogCsvEventHandler::RunAllTestsEnd() {}

void LogCsvEventHandler::TestCaseStart(const TestCase&) { iterations_ = 0; }

void LogCsvEventHandler::TestCaseIteration(const TestIteration&) {
  iterations_ += 1;
}

void LogCsvEventHandler::TestCaseEnd(const TestCase& info,
                                     const TestMeasurement& measurement) {
  // Use long instead of long long since some platforms don't support %lld
  PW_LOG_INFO("%s,%d,%ld,%ld,%ld,%s",
              info.name,
              iterations_,
              static_cast<long>(measurement.min),
              static_cast<long>(measurement.max),
              static_cast<long>(measurement.mean),
              internal::GetDurationUnitStr());
}

}  // namespace pw::perf_test
