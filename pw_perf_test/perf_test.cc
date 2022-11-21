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
#define PW_LOG_MODULE_NAME "pw_perf_test"
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "pw_perf_test/perf_test.h"

#include "pw_log/log.h"

namespace pw::perf_test {

bool State::KeepRunning() {
  internal::Timestamp iteration_end = internal::GetCurrentTimestamp();
  if (current_iteration_ == -1) {
    ++current_iteration_;
    iteration_start_ = internal::GetCurrentTimestamp();
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
  ++current_iteration_;
  PW_LOG_DEBUG("Iteration number: %d - Duration: %lld",
               current_iteration_,
               static_cast<long long>(duration));
  if (current_iteration_ == test_iterations_) {
    PW_LOG_DEBUG("Total Duration: %lld  Total Iterations: %d",
                 static_cast<long long>(total_duration_),
                 test_iterations_);
    mean_ = total_duration_ / test_iterations_;
    PW_LOG_DEBUG("Mean: %lld: ", static_cast<long long>(mean_));
    PW_LOG_DEBUG("Minimum: %lld", static_cast<long long>(min_));
    PW_LOG_DEBUG("Maxmimum: %lld", static_cast<long long>(max_));
    return false;
  }
  iteration_start_ = internal::GetCurrentTimestamp();
  return true;
}

}  // namespace pw::perf_test
