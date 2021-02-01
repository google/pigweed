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
#pragma once

#include <thread>

#include "pw_chrono/system_clock.h"
#include "pw_thread/sleep.h"

namespace pw::this_thread {

inline void sleep_for(chrono::SystemClock::duration for_at_least) {
  for_at_least = std::max(for_at_least, chrono::SystemClock::duration::zero());
  // Although many implementations do yield with sleep_for(0), it is not
  // required, ergo we explicitly add handling.
  if (for_at_least == chrono::SystemClock::duration::zero()) {
    return std::this_thread::yield();
  }
  return std::this_thread::sleep_for(for_at_least);
}

inline void sleep_until(chrono::SystemClock::time_point until_at_least) {
  // Although many implementations do yield with deadlines in the past until
  // the current time, it is not required, ergo we explicitly add handling.
  if (chrono::SystemClock::now() >= until_at_least) {
    return std::this_thread::yield();
  }
  return std::this_thread::sleep_until(until_at_least);
}

}  // namespace pw::this_thread
