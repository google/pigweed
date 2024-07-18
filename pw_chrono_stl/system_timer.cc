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

#include "pw_chrono/system_timer.h"

#include <mutex>
#include <thread>

#include "pw_chrono_stl/system_timer_native.h"

namespace pw::chrono::backend {
namespace internal {

bool NoDepsTimedThreadNotification::try_acquire() {
  std::unique_lock lock(lock_);
  bool was_set = is_set_;
  is_set_ = false;
  return was_set;
}

bool NoDepsTimedThreadNotification::try_acquire_until(
    SystemClock::time_point deadline) {
  std::unique_lock lock(lock_);
  if (cv_.wait_until(lock, deadline, [&] { return is_set_; })) {
    is_set_ = false;
    return true;
  }
  return false;
}

void NoDepsTimedThreadNotification::notify() {
  {
    std::unique_lock lock(lock_);
    is_set_ = true;
  }
  cv_.notify_one();
}

}  // namespace internal

NativeSystemTimer::NativeSystemTimer(internal::ExpiryFn&& callback)
    : timer_state_(
          std::make_shared<internal::TimerState>(std::move(callback))) {
  std::thread thread([state = timer_state_]() {
    while (true) {
      SystemClock::time_point sleep_until;
      {
        std::lock_guard lock(state->lock_);
        if (!state->running_) {
          return;
        }
        // Execute the callback while the expiry deadline is in the past.
        // This avoids an unnecessary unlock, sleep attempt, and relock in
        // the case that the intervals are short or the deadlines are in
        // the past.
        while (state->enabled_ &&
               state->expiry_deadline_ <= SystemClock::now()) {
          // Unset the thread notification.
          state->timer_thread_wakeup_.try_acquire();
          state->enabled_ = false;
          state->callback_(state->expiry_deadline_);
        }
        if (state->enabled_) {
          sleep_until = state->expiry_deadline_;
        } else {
          sleep_until = SystemClock::time_point::max();
        }
      }
      state->timer_thread_wakeup_.try_acquire_until(sleep_until);
    }
  });
  thread.detach();
}

void NativeSystemTimer::InvokeAt(SystemClock::time_point timestamp) {
  {
    std::lock_guard lock(timer_state_->lock_);
    timer_state_->enabled_ = true;
    timer_state_->expiry_deadline_ = timestamp;
  }
  timer_state_->timer_thread_wakeup_.notify();
}

void NativeSystemTimer::Cancel() {
  std::lock_guard lock(timer_state_->lock_);
  timer_state_->enabled_ = false;
}

void NativeSystemTimer::Kill() {
  {
    std::lock_guard lock(timer_state_->lock_);
    timer_state_->enabled_ = false;
    timer_state_->running_ = false;
  }
  timer_state_->timer_thread_wakeup_.notify();
}

}  // namespace pw::chrono::backend
