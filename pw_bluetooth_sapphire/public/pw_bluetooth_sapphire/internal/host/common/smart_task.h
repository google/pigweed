// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SMART_TASK_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SMART_TASK_H_

#include <pw_async/dispatcher.h>
#include <pw_async/task.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"

namespace bt {

// SmartTask is a utility that wraps a pw::async::Task and adds features like
// cancelation upon destruction and state tracking. It is not thread safe, and
// should only be used on the same thread that the dispatcher is running on.
class SmartTask {
 public:
  SmartTask(pw::async::Dispatcher& dispatcher,
            pw::async::TaskFunction&& func = nullptr)
      : dispatcher_(dispatcher), func_(std::move(func)) {}

  ~SmartTask() {
    if (pending_) {
      BT_ASSERT(Cancel());
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

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SMART_TASK_H_
