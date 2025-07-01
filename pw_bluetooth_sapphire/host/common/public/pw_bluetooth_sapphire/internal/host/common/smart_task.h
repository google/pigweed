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

#pragma once
#include <pw_assert/check.h>
#include <pw_async/dispatcher.h>
#include <pw_async/task.h>
#include <pw_function/function.h>

#include <utility>

namespace bt {

// SmartTask is a utility that wraps a pw::async::Task and adds features like
// cancelation upon destruction and state tracking. It is not thread safe, and
// should only be used on the same thread that the dispatcher is running on.
class SmartTask {
 public:
  explicit SmartTask(pw::async::Dispatcher& dispatcher,
                     pw::async::TaskFunction&& func = nullptr)
      : dispatcher_(dispatcher), func_(std::move(func)) {}

  ~SmartTask() {
    if (pending_) {
      PW_CHECK(Cancel());
    }
  }

  void PostAt(pw::chrono::SystemClock::time_point time) {
    pending_ = true;
    dispatcher_.PostAt(task_, time);
  }

  void PostAfter(pw::chrono::SystemClock::duration delay) {
    pending_ = true;
    dispatcher_.PostAfter(task_, delay);
  }

  void Post() {
    pending_ = true;
    dispatcher_.Post(task_);
  }

  bool Cancel() {
    pending_ = false;
    return dispatcher_.Cancel(task_);
  }

  void set_function(pw::async::TaskFunction&& func) { func_ = std::move(func); }

  bool is_pending() const { return pending_; }

  // Returns whether the task has a valid (non-null) callback.
  bool is_valid() const { return func_ != nullptr; }

  pw::async::Dispatcher& dispatcher() const { return dispatcher_; }

 private:
  pw::async::Dispatcher& dispatcher_;
  pw::async::Task task_{[this](pw::async::Context& ctx, pw::Status status) {
    pending_ = false;
    if (func_) {
      func_(ctx, status);
    }
  }};
  pw::async::TaskFunction func_ = nullptr;
  bool pending_ = false;
};

// Choice to disarm or rearm a recurring task.
enum class RecurringDisposition {
  // Rearms the task for execution according to the recurring interval.
  kRecur,
  // Disarms the task for execution until manually rearmed again.
  kFinish,
};

using RecurringTaskFunction =
    pw::Function<RecurringDisposition(pw::async::Context&, pw::Status)>;

// A kind of smart task that goes off at a periodic interval.
class RecurringTask {
 public:
  explicit RecurringTask(pw::async::Dispatcher& dispatcher,
                         pw::chrono::SystemClock::duration interval,
                         RecurringTaskFunction&& func = nullptr)
      : task_(dispatcher, nullptr), interval_(interval) {
    if (func != nullptr) {
      set_function(std::move(func));
    }
  }

  void set_function(RecurringTaskFunction&& func) {
    task_.set_function([this, cb = std::move(func)](pw::async::Context ctx,
                                                    pw::Status status) {
      if (task_.dispatcher().now() < next_deadline_) {
        // Assume that the deadline has changed, so repost and skip the
        // callback.
        PW_CHECK(!is_pending(),
                 "Task just went off so it shouldn't be pending");
        task_.PostAt(next_deadline_);
        return;
      }
      RecurringDisposition choice = cb(ctx, status);
      if (choice == RecurringDisposition::kRecur) {
        ResetTimeout();
      }
    });
  }

  // Updates the deadline to go off at NOW + interval_.
  //
  // If the timer was disarmed, it will rearm it.
  //
  // **Implementation Note:** This is implemented by letting the timer run its
  // course but skipping the callback. This optimizes for the case where the
  // timer is reset multiple times within the same deadline.
  void ResetTimeout() {
    next_deadline_ = task_.dispatcher().now() + interval_;
    if (!is_pending()) {
      task_.PostAt(next_deadline_);
    }
  }

  // Reenables the timer if it had been disabled without pushing the deadline
  // otherwise.
  void Reenable() {
    if (!is_pending()) {
      ResetTimeout();
    }
  }

  void Start() {
    PW_CHECK(task_.is_valid(),
             "Attempted to start a recurring task without setting a callback");
    ResetTimeout();
  }
  bool Cancel() { return task_.Cancel(); }
  bool is_pending() const { return task_.is_pending(); }

 private:
  SmartTask task_;
  pw::chrono::SystemClock::duration interval_;
  pw::chrono::SystemClock::time_point next_deadline_;
};

}  // namespace bt
