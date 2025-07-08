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

#include "pw_async2/waker.h"

#include <mutex>

#include "pw_async2/dispatcher_base.h"

namespace pw::async2 {
namespace internal {

bool CloneWaker(Waker& waker_in, Waker& waker_out, log::Token wait_reason) {
  std::lock_guard lock(impl::dispatcher_lock());
  if (waker_out.task_ != nullptr && waker_out.task_ != waker_in.task_) {
    return false;
  }
  waker_in.InternalCloneIntoLocked(waker_out, wait_reason);
  return true;
}

bool StoreWaker(Context& cx, Waker& waker_out, log::Token wait_reason) {
  return CloneWaker(*cx.waker_, waker_out, wait_reason);
}

}  // namespace internal

Waker::Waker(Waker&& other) noexcept {
  std::lock_guard lock(impl::dispatcher_lock());
  if (other.task_ == nullptr) {
    return;
  }
  Task& task = *other.task_;
  task.RemoveWakerLocked(other);
  task.AddWakerLocked(*this);
}

Waker& Waker::operator=(Waker&& other) noexcept {
  std::lock_guard lock(impl::dispatcher_lock());
  RemoveFromTaskWakerListLocked();
  if (other.task_ == nullptr) {
    return *this;
  }
  Task& task = *other.task_;
  task.RemoveWakerLocked(other);
  task.AddWakerLocked(*this);
  return *this;
}

void Waker::Wake() && {
  std::lock_guard lock(impl::dispatcher_lock());
  if (task_ != nullptr) {
    task_->dispatcher_->WakeTask(*task_);
    RemoveFromTaskWakerListLocked();
  }
}

void Waker::InternalCloneIntoLocked(Waker& out,
                                    [[maybe_unused]] log::Token wait_reason) & {
  // The `out` waker already points to this task, so no work is necessary.
  if (out.task_ == task_) {
    return;
  }
  // Remove the output waker from its existing task's list.
  out.RemoveFromTaskWakerListLocked();
  out.task_ = task_;

#if PW_ASYNC2_DEBUG_WAIT_REASON
  out.wait_reason_ = wait_reason;
#endif  // PW_ASYNC2_DEBUG_WAIT_REASON

  // Only add if the waker being cloned is actually associated with a task.
  if (task_ != nullptr) {
    task_->AddWakerLocked(out);
  }
}

bool Waker::IsEmpty() const {
  std::lock_guard lock(impl::dispatcher_lock());
  return task_ == nullptr;
}

void Waker::InsertIntoTaskWakerList() {
  std::lock_guard lock(impl::dispatcher_lock());
  InsertIntoTaskWakerListLocked();
}

void Waker::InsertIntoTaskWakerListLocked() {
  if (task_ != nullptr) {
    task_->AddWakerLocked(*this);
  }
}

void Waker::RemoveFromTaskWakerList() {
  std::lock_guard lock(impl::dispatcher_lock());
  RemoveFromTaskWakerListLocked();
}

void Waker::RemoveFromTaskWakerListLocked() {
  if (task_ != nullptr) {
    task_->RemoveWakerLocked(*this);
  }
}

}  // namespace pw::async2
