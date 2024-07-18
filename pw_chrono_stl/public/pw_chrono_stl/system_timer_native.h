// Copyright 2021 The Pigweed Authors
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

#include <condition_variable>
#include <memory>
#include <mutex>

#include "pw_chrono/system_clock.h"
#include "pw_function/function.h"

namespace pw::chrono::backend {
namespace internal {

using ExpiryFn = Function<void(SystemClock::time_point expired_deadline)>;

class NoDepsTimedThreadNotification {
 public:
  NoDepsTimedThreadNotification() = default;
  bool try_acquire();
  bool try_acquire_until(SystemClock::time_point deadline);
  void notify();

 private:
  std::mutex lock_;
  std::condition_variable cv_;
  // Guarded by lock_.
  bool is_set_ = false;
};

class TimerState {
 public:
  TimerState(ExpiryFn&& cb) : callback_(std::move(cb)) {}

  NoDepsTimedThreadNotification timer_thread_wakeup_;

  // The mutex is used both to ensure the public API is threadsafe and to
  // ensure that only one expiry callback is executed at time.
  // A recursive mutex is used as the timer callback must be able to invoke
  // its own public API.
  std::recursive_mutex lock_;

  // All guarded by `lock_`;
  const ExpiryFn callback_;
  SystemClock::time_point expiry_deadline_;
  bool enabled_ = false;
  bool running_ = true;
};

}  // namespace internal

class NativeSystemTimer {
 public:
  NativeSystemTimer(internal::ExpiryFn&& callback);
  void InvokeAt(SystemClock::time_point timestamp);
  void Cancel();
  void Kill();

 private:
  // Instead of using a more complex blocking timer cleanup, a shared_pointer is
  // used so that the heap allocation is still valid for the detached threads
  // even after the NativeSystemTimer has been destructed. Note this is shared
  // with all detached threads.
  std::shared_ptr<internal::TimerState> timer_state_;
};

using NativeSystemTimerHandle = NativeSystemTimer&;

}  // namespace pw::chrono::backend
