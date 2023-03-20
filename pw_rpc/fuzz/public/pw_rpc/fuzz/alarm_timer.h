// Copyright 2022 The Pigweed Authors
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

#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"

namespace pw::rpc::fuzz {

/// Represents a timer that invokes a callback on timeout. Once started, it will
/// invoke the callback after a provided duration unless it is restarted,
/// canceled, or destroyed.
class AlarmTimer {
 public:
  AlarmTimer(chrono::SystemTimer::ExpiryCallback&& on_timeout)
      : timer_(std::move(on_timeout)) {}

  chrono::SystemClock::duration timeout() const { return timeout_; }

  /// "Arms" the timer. The callback will be invoked if `timeout` elapses
  /// without a call to `Restart`, `Cancel`, or the destructor. Calling `Start`
  /// again restarts the timer, possibly with a different `timeout` value.
  void Start(chrono::SystemClock::duration timeout) {
    timeout_ = timeout;
    Restart();
  }

  /// Restarts the timer. This is equivalent to calling `Start` with the same
  /// `timeout` as passed previously. Does nothing if `Start` has not been
  /// called.
  void Restart() {
    Cancel();
    timer_.InvokeAfter(timeout_);
  }

  /// "Disarms" the timer. The callback will not be invoked unless `Start` is
  /// called again. Does nothing if `Start` has not been called.
  void Cancel() { timer_.Cancel(); }

 private:
  chrono::SystemTimer timer_;
  chrono::SystemClock::duration timeout_;
};

}  // namespace pw::rpc::fuzz
