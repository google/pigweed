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

// This override header includes the main tokenized logging header and defines
// the PW_LOG macro as the tokenized logging macro.
#pragma once

// This clock relies on the RP2040's native time API, which provides clock
// granularity to 1us.
#define PW_CHRONO_SYSTEM_CLOCK_PERIOD_SECONDS_NUMERATOR 1
#define PW_CHRONO_SYSTEM_CLOCK_PERIOD_SECONDS_DENOMINATOR 1000000

#ifdef __cplusplus

#include "pw_chrono/epoch.h"

namespace pw::chrono::backend {

// The Pico SDK states that the system clock is strictly time-since-boot.
constexpr inline Epoch kSystemClockEpoch = pw::chrono::Epoch::kTimeSinceBoot;

// The Pico's system clock is tied to the watchdog, which is a hardware
// block not tied to a maskable interrupt.
constexpr inline bool kSystemClockNmiSafe = true;

// The Pico's system clock is backed by a hardware block, which means it will
// continue happily even if interrupts are disabled.
constexpr inline bool kSystemClockFreeRunning = true;

}  // namespace pw::chrono::backend

#endif  // __cplusplus
