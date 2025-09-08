// Copyright 2025 The Pigweed Authors
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

#include "item_drop_sensor.h"

#include <mutex>
#include <utility>

namespace codelab {

pw::async2::Poll<> ItemDropSensor::Pend(pw::async2::Context& cx) {
  if (drop_detected_.exchange(false, std::memory_order_relaxed)) {
    return pw::async2::Ready();
  }
  PW_ASYNC_STORE_WAKER(cx, waker_, "item drop");
  return pw::async2::Pending();
}

void ItemDropSensor::Drop() {
  if (!drop_detected_.exchange(true, std::memory_order_relaxed)) {
    std::move(waker_).Wake();
  }
}

}  // namespace codelab
