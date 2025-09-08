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
#pragma once

#include <atomic>

#include "pw_async2/context.h"
#include "pw_async2/waker.h"

namespace codelab {

class ItemDropSensor {
 public:
  constexpr ItemDropSensor() = default;

  // Pends until theitem drop sensor triggers.
  pw::async2::Poll<> Pend(pw::async2::Context& cx);

  // Records an item drop event. Typically called from the drop sensor ISR.
  void Drop();

  // Clears any latched drop events.
  void Clear() { drop_detected_.store(false, std::memory_order_relaxed); }

 private:
  std::atomic<bool> drop_detected_ = false;
  pw::async2::Waker waker_;
};

}  // namespace codelab
