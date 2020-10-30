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

#include <chrono>

#include "pw_chrono/epoch.h"

namespace pw::chrono::backend {

// Provide the native std::chrono::steady_clock period.
using SystemClockPeriodSecondsRatio = std::chrono::steady_clock::period;

// The std::chrono::steady_clock does not have a defined epoch.
constexpr inline Epoch kSystemClockEpoch = pw::chrono::Epoch::kUnknown;

// The std::chrono::steady_clock can be used by signal handlers.
constexpr inline bool kSystemClockNmiSafe = true;

// The std::chrono::steady_clock ticks while in a signal handler.
constexpr inline bool kSystemClockFreeRunning = true;

}  // namespace pw::chrono::backend
