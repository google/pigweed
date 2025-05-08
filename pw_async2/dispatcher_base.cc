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

#include <iterator>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_async2/internal/config.h"
#include "pw_async2/internal/token.h"
#include "pw_async2/waker.h"

#define PW_LOG_MODULE_NAME PW_ASYNC2_CONFIG_LOG_MODULE_NAME
#define PW_LOG_LEVEL PW_ASYNC2_CONFIG_LOG_LEVEL

#include "pw_log/log.h"

namespace pw::async2 {
namespace internal {

void CloneWaker(Waker& waker_in,
                Waker& waker_out,
                internal::Token wait_reason) {
  waker_in.InternalCloneInto(waker_out, wait_reason);
}

void StoreWaker(Context& cx, Waker& waker_out, internal::Token wait_reason) {
  CloneWaker(*cx.waker_, waker_out, wait_reason);
}

}  // namespace internal

void Context::ReEnqueue() {
  Waker waker;
  // The new waker will be immediately woken and removed, so its wait reason
  // does not matter.
  internal::CloneWaker(*waker_, waker);
  std::move(waker).Wake();
}

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
  waker.wait_reason_ = internal::kEmptyToken;
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

void Waker::InternalCloneInto(
    Waker& out, [[maybe_unused]] PW_LOG_TOKEN_TYPE wait_reason) & {
  std::lock_guard lock(impl::dispatcher_lock());
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

void NativeDispatcherBase::Deregister() {
  std::lock_guard lock(impl::dispatcher_lock());
  UnpostTaskList(woken_);
  UnpostTaskList(sleeping_);
}

void NativeDispatcherBase::Post(Task& task) {
  bool wake_dispatcher = false;
  {
    std::lock_guard lock(impl::dispatcher_lock());
    PW_DASSERT(task.state_ == Task::State::kUnposted);
    PW_DASSERT(task.dispatcher_ == nullptr);
    task.state_ = Task::State::kWoken;
    task.dispatcher_ = this;
    woken_.push_back(task);
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
  if (!woken_.empty()) {
    PW_LOG_DEBUG("Dispatcher will not sleep due to nonempty task queue");
    return SleepInfo::DontSleep();
  }
  if (!allow_empty && sleeping_.empty()) {
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
      bool all_complete = woken_.empty() && sleeping_.empty();
      return RunOneTaskResult(
          /*completed_all_tasks=*/all_complete,
          /*completed_main_task=*/false,
          /*ran_a_task=*/false);
    }
    task->state_ = Task::State::kRunning;
  }

  bool complete;
  bool requires_waker;
  {
    Waker waker(*task);
    Context context(dispatcher, waker);
    tasks_polled_.Increment();
    complete = task->Pend(context).IsReady();
    requires_waker = context.requires_waker_;
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
      all_complete = woken_.empty() && sleeping_.empty();
    }
    task->DoDestroy();
    return RunOneTaskResult(
        /*completed_all_tasks=*/all_complete,
        /*completed_main_task=*/task == task_to_look_for,
        /*ran_a_task=*/true);
  }

  std::lock_guard lock(impl::dispatcher_lock());
  if (task->state_ == Task::State::kRunning) {
    if (task->name_ != internal::kEmptyToken) {
      PW_LOG_DEBUG(
          "Dispatcher adding task " PW_LOG_TOKEN_FMT() ":%p to sleep queue",
          task->name_,
          static_cast<const void*>(task));
    } else {
      PW_LOG_DEBUG("Dispatcher adding task (anonymous):%p to sleep queue",
                   static_cast<const void*>(task));
    }

    if (requires_waker) {
      PW_CHECK(!task->wakers_.empty(),
               "Task %p returned Pending() without registering a waker",
               static_cast<const void*>(task));
      task->state_ = Task::State::kSleeping;
      sleeping_.push_front(*task);
    } else {
      // Require the task to be manually re-posted.
      task->state_ = Task::State::kUnposted;
      task->dispatcher_ = nullptr;
    }
  }
  return RunOneTaskResult(
      /*completed_all_tasks=*/false,
      /*completed_main_task=*/false,
      /*ran_a_task=*/true);
}

void NativeDispatcherBase::UnpostTaskList(IntrusiveList<Task>& list) {
  while (!list.empty()) {
    Task& task = list.front();
    task.state_ = Task::State::kUnposted;
    task.dispatcher_ = nullptr;
    task.RemoveAllWakersLocked();
    list.pop_front();
  }
}

void NativeDispatcherBase::RemoveWokenTaskLocked(Task& task) {
  woken_.remove(task);
}

void NativeDispatcherBase::RemoveSleepingTaskLocked(Task& task) {
  sleeping_.remove(task);
}

void NativeDispatcherBase::WakeTask(Task& task) {
  if (task.name_ != internal::kEmptyToken) {
    PW_LOG_DEBUG("Dispatcher waking task " PW_LOG_TOKEN_FMT() ":%p",
                 task.name_,
                 static_cast<const void*>(&task));
  } else {
    PW_LOG_DEBUG("Dispatcher waking task (anonymous):%p",
                 static_cast<const void*>(&task));
  }

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
  woken_.push_back(task);
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
  if (woken_.empty()) {
    return nullptr;
  }
  Task& task = woken_.front();
  woken_.pop_front();
  return &task;
}

void NativeDispatcherBase::LogRegisteredTasks() {
  PW_LOG_INFO("pw::async2::Dispatcher");
  std::lock_guard lock(impl::dispatcher_lock());

  PW_LOG_INFO("Woken tasks:");
  for (const Task& task : woken_) {
    if (task.name_ != internal::kEmptyToken) {
      PW_LOG_INFO("  - " PW_LOG_TOKEN_FMT() ":%p",
                  task.name_,
                  static_cast<const void*>(&task));
    } else {
      PW_LOG_INFO("  - (anonymous):%p", static_cast<const void*>(&task));
    }
  }
  PW_LOG_INFO("Sleeping tasks:");
  for (const Task& task : sleeping_) {
    int waker_count = static_cast<int>(
        std::distance(task.wakers_.begin(), task.wakers_.end()));

    if (task.name_ != internal::kEmptyToken) {
      PW_LOG_INFO("  - " PW_LOG_TOKEN_FMT() ":%p (%d wakers)",
                  task.name_,
                  static_cast<const void*>(&task),
                  waker_count);
    } else {
      PW_LOG_INFO("  - (anonymous):%p (%d wakers)",
                  static_cast<const void*>(&task),
                  waker_count);
    }

#if PW_ASYNC2_DEBUG_WAIT_REASON
    LogTaskWakers(task);
#endif  // PW_ASYNC2_DEBUG_WAIT_REASON
  }
}

#if PW_ASYNC2_DEBUG_WAIT_REASON
void NativeDispatcherBase::LogTaskWakers(const Task& task) {
  int i = 0;
  for (const Waker& waker : task.wakers_) {
    i++;
    if (waker.wait_reason_ != internal::kEmptyToken) {
      PW_LOG_INFO("    * Waker %d: " PW_LOG_TOKEN_FMT("pw_async2"),
                  i,
                  waker.wait_reason_);
    }
  }
}
#endif  // PW_ASYNC2_DEBUG_WAIT_REASON

}  // namespace pw::async2
