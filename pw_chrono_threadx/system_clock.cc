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

#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>

#include "pw_sync/spin_lock.h"
#include "tx_api.h"

namespace pw::chrono::backend {
namespace {

#if defined(TX_NO_TIMER) && TX_NO_TIMER
#error "This backend is not compatible with TX_NO_TIMER"
#endif  // defined(TX_NO_TIMER) && TX_NO_TIMER

// Extension wrapping pw::SpinLock to allow an atomic to be used to determine
// whether it has been constructed and is ready to be used.
class ConstructionSignalingSpinLock : public sync::SpinLock {
 public:
  ConstructionSignalingSpinLock(std::atomic<bool>& constructed_signal)
      : sync::SpinLock() {
    // Note that the memory order is relaxed because C++ global static
    // construction is a single threaded environment.
    constructed_signal.store(true, std::memory_order_relaxed);
  };
};

// This is POD, meaning this atomic is available before static C++ global
// constructors are run.
std::atomic<bool> system_clock_spin_lock_constructed = {false};

ConstructionSignalingSpinLock system_clock_spin_lock(
    system_clock_spin_lock_constructed);

int64_t overflow_tick_count = 0;
ULONG native_tick_count = 0;
static_assert(!SystemClock::is_nmi_safe,
              "global state is not atomic nor double buferred");

// The tick count resets to 0, ergo the overflow count is the max count + 1.
constexpr int64_t kNativeOverflowTickCount =
    static_cast<int64_t>(std::numeric_limits<ULONG>::max()) + 1;

}  // namespace

int64_t GetSystemClockTickCount() {
  // Note that the memory order is relaxed because C++ global static
  // construction is a single threaded environment.
  if (!system_clock_spin_lock_constructed.load(std::memory_order_relaxed)) {
    return tx_time_get();
  }

  std::lock_guard lock(system_clock_spin_lock);
  const ULONG new_native_tick_count = tx_time_get();
  // WARNING: This must be called more than once per overflow period!
  if (new_native_tick_count < native_tick_count) {
    // Native tick count overflow detected!
    overflow_tick_count += kNativeOverflowTickCount;
  }
  native_tick_count = new_native_tick_count;
  return overflow_tick_count + native_tick_count;
}

}  // namespace pw::chrono::backend
