// Copyright 2025 The Pigweed Authors
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

#include "pw_async2/task.h"

#include <mutex>

#include "pw_assert/check.h"
#include "pw_async2/dispatcher_base.h"

namespace pw::async2 {

void Task::RemoveAllWakersLocked() {
  while (!wakers_.empty()) {
    Waker& waker = wakers_.front();
    wakers_.pop_front();
    waker.task_ = nullptr;
  }
}

void Task::AddWakerLocked(Waker& waker) {
  waker.task_ = this;
  wakers_.push_front(waker);
}

void Task::RemoveWakerLocked(Waker& waker) {
  wakers_.remove(waker);
  waker.task_ = nullptr;
#if PW_ASYNC2_DEBUG_WAIT_REASON
  waker.wait_reason_ = log::kDefaultToken;
#endif  // PW_ASYNC2_DEBUG_WAIT_REASON
}

bool Task::IsRegistered() const {
  std::lock_guard lock(impl::dispatcher_lock());
  return state_ != Task::State::kUnposted;
}

void Task::Deregister() {
  pw::sync::Mutex* task_execution_lock;
  {
    // Fast path: the task is not running.
    std::lock_guard lock(impl::dispatcher_lock());
    if (TryDeregister()) {
      return;
    }
    // The task was running, so we have to wait for the task to stop being
    // run by acquiring the `task_lock`.
    task_execution_lock = &dispatcher_->task_execution_lock_;
  }

  // NOTE: there is a race here where `task_execution_lock_` may be
  // invalidated by concurrent destruction of the dispatcher.
  //
  // This restriction is documented above, but is still fairly footgun-y.
  std::lock_guard task_lock(*task_execution_lock);
  std::lock_guard lock(impl::dispatcher_lock());
  PW_CHECK(TryDeregister());
}

bool Task::TryDeregister() {
  switch (state_) {
    case Task::State::kUnposted:
      return true;
    case Task::State::kSleeping:
      dispatcher_->RemoveSleepingTaskLocked(*this);
      break;
    case Task::State::kRunning:
      return false;
    case Task::State::kWoken:
      dispatcher_->RemoveWokenTaskLocked(*this);
      break;
  }
  state_ = Task::State::kUnposted;
  RemoveAllWakersLocked();

  // Wake the dispatcher up if this was the last task so that it can see that
  // all tasks have completed.
  if (dispatcher_->woken_.empty() && dispatcher_->sleeping_.empty() &&
      dispatcher_->wants_wake_) {
    dispatcher_->Wake();
  }
  dispatcher_ = nullptr;
  return true;
}

}  // namespace pw::async2
