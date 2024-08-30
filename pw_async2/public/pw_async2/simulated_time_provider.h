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

#include "pw_async2/time_provider.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace pw::async2 {

/// A simulated `TimeProvider` suitable for testing APIs which use `Timer`.
template <typename Clock>
class SimulatedTimeProvider final : public TimeProvider<Clock> {
 public:
  SimulatedTimeProvider(
      typename Clock::time_point timestamp =
          typename Clock::time_point(typename Clock::duration(0)))
      : now_(timestamp) {}

  /// Advances the simulated time and runs any newly-expired timers.
  void AdvanceTime(typename Clock::duration duration) {
    typename Clock::time_point new_now;
    {
      std::lock_guard lock(lock_);
      now_ += duration;
      new_now = now_;
    }
    TimeProvider<Clock>::RunExpired(new_now);
  }

  /// Modifies the simulated time and runs any newly-expired timers.
  ///
  /// WARNING: Use of this function with a timestamp older than the current
  /// `now()` will violate the is_monotonic clock attribute. We don't like it
  /// when time goes backwards!
  void SetTime(typename Clock::time_point timestamp) {
    {
      std::lock_guard lock(lock_);
      now_ = timestamp;
    }
    TimeProvider<Clock>::RunExpired(timestamp);
  }

  /// Explicitly run expired timers.
  ///
  /// Calls to this function are not usually necessary, as `AdvanceTime` and
  /// `SetTime` will trigger expired timers to run. However, if a timer is set
  /// for a time in the past and neither `AdvanceTime` nor `SetTime` are
  /// subsequently invoked, the timer will not have a chance to run until
  /// one of `AdvanceTime`, `SetTime`, or `RunExpiredTimers` has been called.
  void RunExpiredTimers() { RunExpired(now()); }

  typename Clock::time_point now() const final {
    std::lock_guard lock(lock_);
    return now_;
  }

 private:
  mutable sync::InterruptSpinLock lock_;
  typename Clock::time_point now_ PW_GUARDED_BY(lock_);

  void DoInvokeAt(typename Clock::time_point) final {
    // We don't need to do anything here since calls to `RunExpired` are
    // triggered directly by user calls to `AdvanceTime`/`SetTime`.
    //
    // Note: we cannot run the timer here even if it was in the past because
    // `DoInvokeAt` is called under `TimeProvider`'s lock. Furthermore, we
    // might be *inside* the current callback due to a nested invocation of
    // `InvokeAfter`/`InvokeAt`.
  }

  void DoCancel() final {
    // We don't need to do anything here since `RunExpired` itself is safe to
    // call redundantly-- it will filter out extra invocations.
  }
};

}  // namespace pw::async2
