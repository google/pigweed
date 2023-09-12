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
#include "chre/platform/system_time.h"

#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "pw_chrono/system_clock.h"

namespace chre {

namespace {

int64_t estimated_host_time_offset = 0;

}

Nanoseconds SystemTime::getMonotonicTime() {
  const pw::chrono::SystemClock::time_point now =
      pw::chrono::SystemClock::now();
  auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   now.time_since_epoch())
                   .count();
  return Nanoseconds(static_cast<uint64_t>(nsecs));
}

int64_t SystemTime::getEstimatedHostTimeOffset() {
  return estimated_host_time_offset;
}

void SystemTime::setEstimatedHostTimeOffset(int64_t offset) {
  estimated_host_time_offset = offset;
}

}  // namespace chre
