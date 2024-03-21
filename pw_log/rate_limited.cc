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

#include "pw_log/rate_limited.h"

namespace pw::log::internal {

RateLimiter::PollResult RateLimiter::Poll(
    chrono::SystemClock::duration min_interval_between_logs) {
  PollResult result({
      .count = 0,
      .logs_per_s = 0,
  });
  const chrono::SystemClock::time_point now = chrono::SystemClock::now();
  const chrono::SystemClock::duration elapsed = now - last_timestamp_;

  if (count_ < std::numeric_limits<uint16_t>::max()) {
    count_++;
  }

  if (last_timestamp_.time_since_epoch().count() != 0 &&
      elapsed < min_interval_between_logs) {
    return result;
  }

  // Add half to round not floor.
  uint16_t elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  if (elapsed_ms != 0) {
    result.logs_per_s = (count_ * 1000 + 500) / elapsed_ms;
  }
  result.count = count_;

  last_timestamp_ = now;
  count_ = 0;

  return result;
}

}  // namespace pw::log::internal