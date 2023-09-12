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
#include "chre/platform/system_timer.h"

#include "chre/platform/log.h"
#include "chre/util/time.h"

namespace chre {

void SystemTimerBase::OnExpired() {
  SystemTimer* timer = static_cast<SystemTimer*>(this);
  timer->mCallback(timer->mData);
}

SystemTimer::SystemTimer() {}

SystemTimer::~SystemTimer() {
  if (!initialized_) {
    return;
  }
  cancel();
  initialized_ = false;
}

bool SystemTimer::init() {
  initialized_ = true;
  return initialized_;
}

bool SystemTimer::set(SystemTimerCallback* callback,
                      void* data,
                      Nanoseconds delay) {
  if (!initialized_) {
    return false;
  }
  mCallback = callback;
  mData = data;
  pw::chrono::SystemClock::duration interval =
      std::chrono::nanoseconds(delay.toRawNanoseconds());
  const pw::chrono::SystemClock::time_point now =
      pw::chrono::SystemClock::now();
  timer_.InvokeAt(now + interval);
  return true;
}

bool SystemTimer::cancel() {
  if (!initialized_) {
    return false;
  }
  timer_.Cancel();
  return true;
}

bool SystemTimer::isActive() { return is_active_; }

}  // namespace chre
