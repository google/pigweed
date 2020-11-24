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

#include <stddef.h>
#include <stdint.h>

#include "pw_preprocessor/util.h"

#ifdef __cplusplus

#include <chrono>

// The backend implements this header to provide the following SystemClock
// parameters, for more detail on the parameters see the SystemClock usage of
// them below:
//   std::ratio<> typed pw::chrono::backend::SystemClockPeriodSecondsRatio type
//   constexpr pw::chrono::Epoch pw::chrono::backend::kSystemClockEpoch;
//   constexpr bool pw::chrono::backend::kSystemClockFreeRunning;
//   constexpr bool pw::chrono::backend::kSystemClockNmiSafe;
#include "pw_chrono_backend/system_clock_config.h"

namespace pw::chrono {
namespace backend {

// The ARM AEBI does not permit the opaque 'time_point' to be passed via
// registers, ergo the underlying fundamental type is forward declared.
// A SystemCLock tick has the units of one SystemClock::period duration.
// This must be thread and IRQ safe and provided by the backend.
int64_t GetSystemClockTickCount();

}  // namespace backend

// The SystemClock represents an unsteady, monotonic clock.
//
// The epoch of this clock is unspecified and may not be related to wall time
// (for example, it can be time since boot). The time between ticks of this
// clock may vary due to sleep modes and potential interrupt handling.
// SystemClock meets the requirements of C++'s TrivialClock and Pigweed's
// PigweedClock.
//
// SystemClock is compatible with C++'s Clock & TrivialClock including:
//   SystemClock::rep
//   SystemClock::period
//   SystemClock::duration
//   SystemClock::time_point
//   SystemClock::is_steady
//   SystemClock::now()
//
// Example:
//
//   SystemClock::time_point before = SystemClock::now();
//   TakesALongTime();
//   SystemClock::duration time_taken = SystemClock::now() - before;
//   bool took_way_too_long = false;
//   if (time_taken > std::chrono::seconds(42)) {
//     took_way_too_long = true;
//   }
//
// This code is thread & IRQ safe, it may be NMI safe depending on is_nmi_safe.
struct SystemClock {
  using rep = int64_t;
  // The period must be provided by the backend.
  using period = backend::SystemClockPeriodSecondsRatio;
  using duration = std::chrono::duration<rep, period>;
  using time_point = std::chrono::time_point<SystemClock>;
  // The epoch must be provided by the backend.
  static constexpr Epoch epoch = backend::kSystemClockEpoch;

  // The time points of this clock cannot decrease, however the time between
  // ticks of this clock may slightly vary due to sleep modes. The duration
  // during sleep may be ignored or backfilled with another clock.
  static constexpr bool is_monotonic = true;
  static constexpr bool is_steady = false;

  // The now() function may not move forward while in a critical section or
  // interrupt. This must be provided by the backend.
  static constexpr bool is_free_running = backend::kSystemClockFreeRunning;

  // The clock must stop while in halting debug mode.
  static constexpr bool is_stopped_in_halting_debug_mode = true;

  // The now() function can be invoked at any time.
  static constexpr bool is_always_enabled = true;

  // The now() function may work in non-masking interrupts, depending on the
  // backend. This must be provided by the backend.
  static constexpr bool is_nmi_safe = backend::kSystemClockNmiSafe;

  // This is thread and IRQ safe. This must be provided by the backend.
  static time_point now() {
    return time_point(duration(backend::GetSystemClockTickCount()));
  }
};

// An abstract interface representing a SystemClock.
//
// This interface allows decoupling code that uses time from the code that
// creates a point in time. You can use this to your advantage by injecting
// Clocks into interfaces rather than having implementations call
// SystemClock::now() directly. However, this comes at a cost of a vtable per
// implementation and more importantly passing and maintaining references to the
// VirtualSystemCLock for all of the users.
//
// The VirtualSystemClock::RealClock() function returns a reference to the
// real global SystemClock.
//
// Example:
//
//  void DoFoo(VirtualSystemClock& system_clock) {
//    SystemClock::time_point now = clock.now();
//    // ... Code which consumes now.
//  }
//
//  // Production code:
//  DoFoo(VirtualSystemCLock::RealClock);
//
//  // Test code:
//  MockClock test_clock();
//  DoFoo(test_clock);
//
// This interface is thread and IRQ safe.
class VirtualSystemClock {
 public:
  // Returns a reference to the real system clock to aid instantiation.
  static VirtualSystemClock& RealClock();

  virtual ~VirtualSystemClock() = default;
  virtual SystemClock::time_point now() = 0;
};

}  // namespace pw::chrono

// The backend can opt to include an inlined implementation of the following:
//   int64_t GetSystemClockTickCount();
#if __has_include("pw_chrono_backend/system_clock_inline.h")
#include "pw_chrono_backend/system_clock_inline.h"
#endif  // __has_include("pw_chrono_backend/system_clock_inline.h")

#endif  // __cplusplus

PW_EXTERN_C_START

typedef int64_t pw_chrono_SystemClock_TickCount;
typedef struct {
  pw_chrono_SystemClock_TickCount ticks_since_epoch;
} pw_chrono_SystemClock_TimePoint;
typedef int64_t pw_chrono_SystemClock_Nanoseconds;

// Returns the current time, see SystemClock::now() for more detail.
pw_chrono_SystemClock_TimePoint pw_chrono_SystemClock_Now();

// Returns the change in time between the current_time - last_time.
pw_chrono_SystemClock_TickCount pw_chrono_SystemClock_TimeDelta(
    pw_chrono_SystemClock_TimePoint last_time,
    pw_chrono_SystemClock_TimePoint current_time);

// For lossless time unit conversion, the seconds per tick ratio that is
// numerator/denominator should be used.
int32_t pw_chrono_SystemClock_PeriodSeconds_Numerator();
int32_t pw_chrono_SystemClock_PeriodSeconds_Denominator();

// Warning, this may be lossy due to the use of std::chrono::duration_cast,
// rounding towards zero.
pw_chrono_SystemClock_Nanoseconds pw_chrono_SystemClock_TickCountToNsTruncate(
    pw_chrono_SystemClock_TickCount ticks);

PW_EXTERN_C_END
