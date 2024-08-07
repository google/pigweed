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

#include <memory>

#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"

namespace pw::chrono {

inline SystemTimer::SystemTimer(ExpiryCallback&& callback)
    : native_type_(std::move(callback)) {}

inline SystemTimer::~SystemTimer() { native_type_.Kill(); }

inline void SystemTimer::InvokeAfter(SystemClock::duration delay) {
  InvokeAt(SystemClock::TimePointAfterAtLeast(delay));
}

inline void SystemTimer::InvokeAt(SystemClock::time_point timestamp) {
  native_type_.InvokeAt(timestamp);
}

inline void SystemTimer::Cancel() { native_type_.Cancel(); }

inline SystemTimer::native_handle_type SystemTimer::native_handle() {
  return native_type_;
}

}  // namespace pw::chrono
