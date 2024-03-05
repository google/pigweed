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

#include "pw_async2/dispatcher_base.h"

#include <mutex>

#include "pw_assert/check.h"
#include "pw_sync/lock_annotations.h"

namespace pw::async2 {

void Context::ReEnqueue() { waker_->Clone(WaitReason::Unspecified()).Wake(); }

Waker Context::GetWaker(WaitReason reason) { return waker_->Clone(reason); }

void Task::RemoveAllWakersLocked() {
  while (wakers_ != nullptr) {
    Waker* current = wakers_;
    wakers_ = current->next_;
    current->task_ = nullptr;
    current->next_ = nullptr;
  }
}

void Task::AddWakerLocked(Waker& waker) {
  waker.task_ = this;
  waker.next_ = wakers_;
  wakers_ = &waker;
}

void Task::RemoveWakerLocked(Waker& waker) {
  if (&waker == wakers_) {
    wakers_ = wakers_->next_;
  } else {
    Waker* current = wakers_;
    while (current->next_ != &waker) {
      current = current->next_;
    }
    current->next_ = current->next_->next_;
  }
  waker.task_ = nullptr;
  waker.next_ = nullptr;
}

Waker::Waker(Waker&& other) noexcept {
  std::lock_guard lock(dispatcher_lock());
  if (other.task_ == nullptr) {
    return;
  }
  Task& task = *other.task_;
  task.RemoveWakerLocked(other);
  task.AddWakerLocked(*this);
}

Waker& Waker::operator=(Waker&& other) noexcept {
  std::lock_guard lock(dispatcher_lock());
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
  std::lock_guard lock(dispatcher_lock());
  if (task_ != nullptr) {
    task_->dispatcher_->WakeTask(*task_);
    RemoveFromTaskWakerListLocked();
  }
}

Waker Waker::Clone(WaitReason) & {
  Waker waker;
  {
    std::lock_guard lock(dispatcher_lock());
    if (task_ != nullptr) {
      waker.task_ = task_;
      task_->AddWakerLocked(waker);
    }
  }
  return waker;
}

bool Waker::IsEmpty() const {
  std::lock_guard lock(dispatcher_lock());
  return task_ == nullptr;
}

void Waker::InsertIntoTaskWakerList() {
  std::lock_guard lock(dispatcher_lock());
  InsertIntoTaskWakerListLocked();
}

void Waker::InsertIntoTaskWakerListLocked() {
  if (task_ != nullptr) {
    task_->AddWakerLocked(*this);
  }
}

void Waker::RemoveFromTaskWakerList() {
  std::lock_guard lock(dispatcher_lock());
  RemoveFromTaskWakerListLocked();
}

void Waker::RemoveFromTaskWakerListLocked() {
  if (task_ != nullptr) {
    task_->RemoveWakerLocked(*this);
  }
}

void DispatcherBase::Deregister() {
  std::lock_guard lock(dispatcher_lock());
  UnpostTaskList(first_woken_);
  first_woken_ = nullptr;
  last_woken_ = nullptr;
  UnpostTaskList(sleeping_);
  sleeping_ = nullptr;
}

void DispatcherBase::UnpostTaskList(Task* task) {
  while (task != nullptr) {
    task->state_ = Task::State::kUnposted;
    task->dispatcher_ = nullptr;
    task->prev_ = nullptr;
    Task* next = task->next_;
    task->next_ = nullptr;
    task->RemoveAllWakersLocked();
    task = next;
  }
}

void DispatcherBase::RemoveTaskFromList(Task& task) {
  if (task.prev_ != nullptr) {
    task.prev_->next_ = task.next_;
  }
  if (task.next_ != nullptr) {
    task.next_->prev_ = task.prev_;
  }
  task.prev_ = nullptr;
  task.next_ = nullptr;
}

void DispatcherBase::RemoveWokenTaskLocked(Task& task) {
  RemoveTaskFromList(task);
  if (first_woken_ == &task) {
    first_woken_ = task.next_;
  }
  if (last_woken_ == &task) {
    last_woken_ = task.prev_;
  }
}

void DispatcherBase::RemoveSleepingTaskLocked(Task& task) {
  RemoveTaskFromList(task);
  if (sleeping_ == &task) {
    sleeping_ = task.next_;
  }
}

void DispatcherBase::AddTaskToWokenList(Task& task) {
  if (first_woken_ == nullptr) {
    first_woken_ = &task;
  } else {
    last_woken_->next_ = &task;
    task.prev_ = last_woken_;
  }
  last_woken_ = &task;
}

void DispatcherBase::AddTaskToSleepingList(Task& task) {
  if (sleeping_ != nullptr) {
    sleeping_->prev_ = &task;
  }
  task.next_ = sleeping_;
  sleeping_ = &task;
}

void DispatcherBase::WakeTask(Task& task) {
  switch (task.state_) {
    case Task::State::kWoken:
      // Do nothing-- this has already been woken.
      return;
    case Task::State::kUnposted:
      // This should be unreachable.
      PW_CHECK(false);
    case Task::State::kRunning:
      // Wake again to indicate that this task should be run once more,
      // as the state of the world may have changed since the task
      // started running.
      break;
    case Task::State::kSleeping:
      RemoveSleepingTaskLocked(task);
      // Wake away!
      break;
  }
  task.state_ = Task::State::kWoken;
  AddTaskToWokenList(task);
  if (wants_wake_) {
    // Note: it's quite annoying to make this call under the lock, as it can
    // result in extra thread wakeup/sleep cycles.
    //
    // However, releasing the lock first would allow for the possibility that
    // the ``Dispatcher`` has been destroyed, making the call invalid.
    DoWake();
  }
}

Task* DispatcherBase::PopWokenTask() {
  if (first_woken_ == nullptr) {
    return nullptr;
  }
  Task& task = *first_woken_;
  if (task.next_ != nullptr) {
    task.next_->prev_ = nullptr;
  } else {
    last_woken_ = nullptr;
  }
  first_woken_ = task.next_;
  task.prev_ = nullptr;
  task.next_ = nullptr;
  return &task;
}

}  // namespace pw::async2
