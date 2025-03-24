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
#include "pw_async2/internal/config.h"

#define PW_LOG_MODULE_NAME PW_ASYNC2_CONFIG_LOG_MODULE_NAME
#define PW_LOG_LEVEL PW_ASYNC2_CONFIG_LOG_LEVEL

#include "pw_log/log.h"

namespace pw::async2 {

void Context::ReEnqueue() {
  Waker waker;
  waker_->InternalCloneInto(waker);
  std::move(waker).Wake();
}

void Context::InternalStoreWaker(Waker& waker_out) {
  waker_->InternalCloneInto(waker_out);
}

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
  if (dispatcher_->first_woken_ == nullptr &&
      dispatcher_->sleeping_ == nullptr && dispatcher_->wants_wake_) {
    dispatcher_->Wake();
  }
  dispatcher_ = nullptr;
  return true;
}

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

void Waker::InternalCloneInto(Waker& out) & {
  std::lock_guard lock(impl::dispatcher_lock());
  // The `out` waker already points to this task, so no work is necessary.
  if (out.task_ == task_) {
    return;
  }
  // Remove the output waker from its existing task's list.
  out.RemoveFromTaskWakerListLocked();
  out.task_ = task_;
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

void NativeDispatcherBase::Deregister() {
  std::lock_guard lock(impl::dispatcher_lock());
  UnpostTaskList(first_woken_);
  first_woken_ = nullptr;
  last_woken_ = nullptr;
  UnpostTaskList(sleeping_);
  sleeping_ = nullptr;
}

void NativeDispatcherBase::Post(Task& task) {
  bool wake_dispatcher = false;
  {
    std::lock_guard lock(impl::dispatcher_lock());
    PW_DASSERT(task.state_ == Task::State::kUnposted);
    PW_DASSERT(task.dispatcher_ == nullptr);
    task.state_ = Task::State::kWoken;
    task.dispatcher_ = this;
    AddTaskToWokenList(task);
    if (wants_wake_) {
      wake_dispatcher = true;
      wants_wake_ = false;
    }
  }
  // Note: unlike in ``WakeTask``, here we know that the ``Dispatcher`` will
  // not be destroyed out from under our feet because we're in a method being
  // called on the ``Dispatcher`` by a user.
  if (wake_dispatcher) {
    Wake();
  }
}

NativeDispatcherBase::SleepInfo NativeDispatcherBase::AttemptRequestWake(
    bool allow_empty) {
  std::lock_guard lock(impl::dispatcher_lock());
  // Don't allow sleeping if there are already tasks waiting to be run.
  if (first_woken_ != nullptr) {
    PW_LOG_DEBUG("Dispatcher will not sleep due to nonempty task queue");
    return SleepInfo::DontSleep();
  }
  if (!allow_empty && sleeping_ == nullptr) {
    PW_LOG_DEBUG("Dispatcher will not sleep due to empty sleep queue");
    return SleepInfo::DontSleep();
  }
  /// Indicate that the ``Dispatcher`` is sleeping and will need a ``DoWake``
  /// call once more work can be done.
  wants_wake_ = true;
  sleep_count_.Increment();
  // Once timers are added, this should check them.
  return SleepInfo::Indefinitely();
}

NativeDispatcherBase::RunOneTaskResult NativeDispatcherBase::RunOneTask(
    Dispatcher& dispatcher, Task* task_to_look_for) {
  std::lock_guard task_lock(task_execution_lock_);
  Task* task;
  {
    std::lock_guard lock(impl::dispatcher_lock());
    task = PopWokenTask();
    if (task == nullptr) {
      PW_LOG_DEBUG("Dispatcher has no woken tasks to run");
      bool all_complete = first_woken_ == nullptr && sleeping_ == nullptr;
      return RunOneTaskResult(
          /*completed_all_tasks=*/all_complete,
          /*completed_main_task=*/false,
          /*ran_a_task=*/false);
    }
    task->state_ = Task::State::kRunning;
  }

  bool complete;
  {
    Waker waker(*task);
    Context context(dispatcher, waker);
    tasks_polled_.Increment();
    complete = task->Pend(context).IsReady();
  }
  if (complete) {
    tasks_completed_.Increment();
    bool all_complete;
    {
      std::lock_guard lock(impl::dispatcher_lock());
      switch (task->state_) {
        case Task::State::kUnposted:
        case Task::State::kSleeping:
          PW_DASSERT(false);
          PW_UNREACHABLE;
        case Task::State::kRunning:
          break;
        case Task::State::kWoken:
          RemoveWokenTaskLocked(*task);
          break;
      }
      task->state_ = Task::State::kUnposted;
      task->dispatcher_ = nullptr;
      task->RemoveAllWakersLocked();
      all_complete = first_woken_ == nullptr && sleeping_ == nullptr;
    }
    task->DoDestroy();
    return RunOneTaskResult(
        /*completed_all_tasks=*/all_complete,
        /*completed_main_task=*/task == task_to_look_for,
        /*ran_a_task=*/true);
  } else {
    std::lock_guard lock(impl::dispatcher_lock());
    if (task->state_ == Task::State::kRunning) {
      PW_LOG_DEBUG("Dispatcher adding task %p to sleep queue",
                   static_cast<const void*>(task));
      task->state_ = Task::State::kSleeping;
      AddTaskToSleepingList(*task);
    }
    return RunOneTaskResult(
        /*completed_all_tasks=*/false,
        /*completed_main_task=*/false,
        /*ran_a_task=*/true);
  }
}

void NativeDispatcherBase::UnpostTaskList(Task* task) {
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

void NativeDispatcherBase::RemoveTaskFromList(Task& task) {
  if (task.prev_ != nullptr) {
    task.prev_->next_ = task.next_;
  }
  if (task.next_ != nullptr) {
    task.next_->prev_ = task.prev_;
  }
  task.prev_ = nullptr;
  task.next_ = nullptr;
}

void NativeDispatcherBase::RemoveWokenTaskLocked(Task& task) {
  if (first_woken_ == &task) {
    first_woken_ = task.next_;
  }
  if (last_woken_ == &task) {
    last_woken_ = task.prev_;
  }
  RemoveTaskFromList(task);
}

void NativeDispatcherBase::RemoveSleepingTaskLocked(Task& task) {
  if (sleeping_ == &task) {
    sleeping_ = task.next_;
  }
  RemoveTaskFromList(task);
}

void NativeDispatcherBase::AddTaskToWokenList(Task& task) {
  if (first_woken_ == nullptr) {
    first_woken_ = &task;
  } else {
    last_woken_->next_ = &task;
    task.prev_ = last_woken_;
  }
  last_woken_ = &task;
}

void NativeDispatcherBase::AddTaskToSleepingList(Task& task) {
  if (sleeping_ != nullptr) {
    sleeping_->prev_ = &task;
  }
  task.next_ = sleeping_;
  sleeping_ = &task;
}

void NativeDispatcherBase::WakeTask(Task& task) {
  PW_LOG_DEBUG("Dispatcher waking task %p", static_cast<const void*>(&task));

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
    Wake();
  }
}

Task* NativeDispatcherBase::PopWokenTask() {
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

void NativeDispatcherBase::LogRegisteredTasks() {
  PW_LOG_INFO("pw::async2::Dispatcher");
  std::lock_guard lock(impl::dispatcher_lock());

  PW_LOG_INFO("Woken tasks:");
  for (Task* task = first_woken_; task != nullptr; task = task->next_) {
    PW_LOG_INFO("  - Task %p", static_cast<const void*>(task));
  }
  PW_LOG_INFO("Sleeping tasks:");
  for (Task* task = sleeping_; task != nullptr; task = task->next_) {
    int waker_count = 0;
    for (Waker* waker = task->wakers_; waker != nullptr; waker = waker->next_) {
      waker_count++;
    }

    PW_LOG_INFO(
        "  - Task %p (%d wakers)", static_cast<const void*>(task), waker_count);
  }
}

}  // namespace pw::async2
