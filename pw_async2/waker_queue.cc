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

#include "pw_async2/waker_queue.h"

#include <mutex>

#include "pw_log/tokenized_args.h"

namespace pw::async2::internal {

bool StoreWaker(Context& cx, WakerQueueBase& queue, log::Token wait_reason) {
  Waker waker;
  if (!CloneWaker(*cx.waker_, waker, wait_reason)) {
    return false;
  }
  return queue.Add(std::move(waker));
}

void WakerQueueBase::WakeMany(size_t count) {
  while (count > 0 && !empty()) {
    Waker& waker = queue_.front();
    std::move(waker).Wake();
    queue_.pop();
    count--;
  }
}

bool WakerQueueBase::Add(Waker&& waker)
    PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
  if (waker.IsEmpty()) {
    return false;
  }

  {
    // Don't store multiple wakers for the same task.
    std::lock_guard lock(impl::dispatcher_lock());
    for (Waker& queued_waker : queue_) {
      if (waker.task_ == queued_waker.task_) {
        return true;
      }
    }
  }

  if (queue_.full()) {
    return false;
  }

  queue_.emplace(std::move(waker));
  return true;
}

}  // namespace pw::async2::internal
