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

#include "pw_chrono/system_clock.h"

namespace pw::chrono {
namespace {

class RealSystemClock final : public VirtualSystemClock {
 public:
  SystemClock::time_point now() final { return SystemClock::now(); }
};
RealSystemClock real_system_clock;

}  // namespace

VirtualSystemClock& VirtualSystemClock::RealClock() {
  return real_system_clock;
}

}  // namespace pw::chrono

extern "C" pw_chrono_SystemClock_TimePoint pw_chrono_SystemClock_Now() {
  return {.ticks_since_epoch =
              pw::chrono::SystemClock::now().time_since_epoch().count()};
}

extern "C" pw_chrono_SystemClock_TickCount pw_chrono_SystemClock_TimeDelta(
    pw_chrono_SystemClock_TimePoint last_time,
    pw_chrono_SystemClock_TimePoint current_time) {
  return current_time.ticks_since_epoch - last_time.ticks_since_epoch;
}

extern "C" int32_t pw_chrono_SystemClock_PeriodSeconds_Numerator() {
  return pw::chrono::SystemClock::period::num;
}

extern "C" int32_t pw_chrono_SystemClock_PeriodSeconds_Denominator() {
  return pw::chrono::SystemClock::period::den;
}

extern "C" pw_chrono_SystemClock_Nanoseconds
pw_chrono_SystemClock_TickCountToNsTruncate(
    pw_chrono_SystemClock_TickCount ticks) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             pw::chrono::SystemClock::duration(ticks))
      .count();
}
