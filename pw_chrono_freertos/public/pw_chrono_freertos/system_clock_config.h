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

#include <ratio>

#include "FreeRTOS.h"
#include "pw_chrono/epoch.h"

namespace pw::chrono::backend {

// Use the FreeRTOS config's tick rate.
using SystemClockPeriodSecondsRatio = std::ratio<1, configTICK_RATE_HZ>;

// The FreeRTOS clock starts at zero during initialization, approximately the
// time since boot.
constexpr inline Epoch kSystemClockEpoch = pw::chrono::Epoch::kTimeSinceBoot;

// The current backend implementation is not NMI safe.
constexpr inline bool kSystemClockNmiSafe = false;

// The FreeRTOS clock halts when the systick interrupt is masked.
constexpr inline bool kSystemClockFreeRunning = false;

}  // namespace pw::chrono::backend
