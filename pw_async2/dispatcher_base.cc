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
#include "pw_async2/waker.h"
#include "pw_log/tokenized_args.h"

#define PW_LOG_MODULE_NAME PW_ASYNC2_CONFIG_LOG_MODULE_NAME
#define PW_LOG_LEVEL PW_ASYNC2_CONFIG_LOG_LEVEL

#include "pw_log/log.h"

namespace pw::async2 {

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
    PW_LOG_DEBUG(
        "Dispatcher adding task " PW_LOG_TOKEN_FMT() ":%p to sleep queue",
        task->name_,
        static_cast<const void*>(task));

    if (requires_waker) {
      PW_CHECK(!task->wakers_.empty(),
               "Task " PW_LOG_TOKEN_FMT()
               ":%p returned Pending() without registering a waker",
               task->name_, static_cast<const void*>(task));
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
  PW_LOG_DEBUG("Dispatcher waking task " PW_LOG_TOKEN_FMT() ":%p",
               task.name_,
               static_cast<const void*>(&task));

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
    PW_LOG_INFO("  - " PW_LOG_TOKEN_FMT() ":%p",
                task.name_,
                static_cast<const void*>(&task));
  }
  PW_LOG_INFO("Sleeping tasks:");
  for (const Task& task : sleeping_) {
    int waker_count = static_cast<int>(
        std::distance(task.wakers_.begin(), task.wakers_.end()));

    PW_LOG_INFO("  - " PW_LOG_TOKEN_FMT() ":%p (%d wakers)",
                task.name_,
                static_cast<const void*>(&task),
                waker_count);

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
    if (waker.wait_reason_ != log::kDefaultToken) {
      PW_LOG_INFO("    * Waker %d: " PW_LOG_TOKEN_FMT("pw_async2"),
                  i,
                  waker.wait_reason_);
    }
  }
}
#endif  // PW_ASYNC2_DEBUG_WAIT_REASON

}  // namespace pw::async2
