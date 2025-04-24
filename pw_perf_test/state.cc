// Copyright 2023 The Pigweed Authors
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

#include "pw_perf_test/state.h"

#include "pw_log/log.h"
#include "pw_numeric/integer_division.h"

namespace pw::perf_test {
namespace internal {

State CreateState(int durations,
                  EventHandler& event_handler,
                  const char* test_name) {
  return State(durations, event_handler, test_name);
}

}  // namespace internal

bool State::KeepRunningInternal(internal::Timestamp iteration_end) {
  current_iteration_ += 1;
  if (current_iteration_ < 0) {
    return true;  // Do nothing for warm up iterations.
  }

  // Send the TestCaseStart event before the first iteration.
  if (current_iteration_ == 0) {
    event_handler_->TestCaseStart(test_info);
    return true;
  }

  int64_t duration = internal::GetDuration(iteration_start_, iteration_end);
  if (duration > max_) {
    max_ = duration;
  }
  if (duration < min_) {
    min_ = duration;
  }
  total_duration_ += duration;
  PW_LOG_DEBUG("Iteration number: %d - Duration: %ld",
               current_iteration_,
               static_cast<long>(duration));
  event_handler_->TestCaseIteration({static_cast<uint32_t>(current_iteration_),
                                     static_cast<float>(duration)});

  if (current_iteration_ < test_iterations_) {
    return true;
  }

  // Final iteration
  PW_LOG_DEBUG("Total Duration: %ld  Total Iterations: %d",
               static_cast<long>(total_duration_),
               test_iterations_);
  mean_ =
      IntegerDivisionRoundNearest(total_duration_, int64_t{test_iterations_});
  PW_LOG_DEBUG("Mean: %ld", static_cast<long>(mean_));
  PW_LOG_DEBUG("Minimum: %ld", static_cast<long>(min_));
  PW_LOG_DEBUG("Maximum: %ld", static_cast<long>(max_));
  TestMeasurement test_measurement = {
      .mean = mean_,
      .max = max_,
      .min = min_,
  };
  event_handler_->TestCaseEnd(test_info, test_measurement);
  return false;
}

}  // namespace pw::perf_test
