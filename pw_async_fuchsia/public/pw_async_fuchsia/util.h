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
#pragma once

#include <lib/zx/time.h>

#include "pw_chrono/system_clock.h"

namespace pw::async_fuchsia {

constexpr pw::chrono::SystemClock::time_point ZxTimeToTimepoint(zx::time time) {
  timespec ts = time.to_timespec();
  auto duration =
      std::chrono::seconds{ts.tv_sec} + std::chrono::nanoseconds{ts.tv_nsec};
  return pw::chrono::SystemClock::time_point{duration};
}

constexpr zx::time TimepointToZxTime(pw::chrono::SystemClock::time_point tp) {
  auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(tp);
  auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) -
            std::chrono::time_point_cast<std::chrono::nanoseconds>(seconds);

  return zx::time{timespec{seconds.time_since_epoch().count(), ns.count()}};
}

}  // namespace pw::async_fuchsia
