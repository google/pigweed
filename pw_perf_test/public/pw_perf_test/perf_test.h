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
#pragma once

#include <limits>

#include "pw_assert/assert.h"
#include "pw_perf_test/internal/timer.h"

namespace pw::perf_test {

class State {
 public:
  // Make private once macro changes are released
  constexpr State(int iterations)
      : mean_(-1),
        test_iterations_(iterations),
        total_duration_(0),
        min_(std::numeric_limits<int64_t>::max()),
        max_(std::numeric_limits<int64_t>::min()),
        current_iteration_(-1) {
    PW_ASSERT(test_iterations_ > 0);
  }
  // KeepRunning() should be called in a while loop. Responsible for managing
  // iterations and timestamps.
  bool KeepRunning();

 private:
  // Set public after deciding how exactly to set user-defined iterations
  void SetIterations(int iterations) {
    PW_ASSERT(current_iteration_ == -1);
    test_iterations_ = iterations;
    PW_ASSERT(test_iterations_ > 0);
  }

  int64_t mean_;

  // Stores the total number of iterations wanted
  int test_iterations_;

  // Stores the total duration of the tests.
  int64_t total_duration_;

  // Smallest value of the iterations
  int64_t min_;

  // Largest value of the iterations
  int64_t max_;

  // Time at the start of the iteration
  internal::Timestamp iteration_start_;

  // The current iteration
  int current_iteration_;
};

}  // namespace pw::perf_test
