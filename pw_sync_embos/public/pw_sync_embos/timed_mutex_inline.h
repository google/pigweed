// Copyright 2021 The Pigweed Authors
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

#include "pw_chrono/system_clock.h"
#include "pw_sync/timed_mutex.h"

namespace pw::sync {

inline bool TimedMutex::try_lock_until(
    chrono::SystemClock::time_point until_at_least) {
  // Note that if this deadline is in the future, it will get rounded up by
  // one whole tick due to how try_lock_for is implemented.
  return try_lock_for(until_at_least - chrono::SystemClock::now());
}

}  // namespace pw::sync
