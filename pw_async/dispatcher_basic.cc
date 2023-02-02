// Copyright 2022 The Pigweed Authors
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
#include "pw_async/dispatcher_basic.h"

#include "pw_log/log.h"

using namespace std::chrono_literals;

namespace pw::async {

const chrono::SystemClock::duration SLEEP_DURATION = 5s;

void BasicDispatcher::Run() {
  lock_.lock();
  while (!stop_requested_) {
    RunLoopOnce();
  }
  lock_.unlock();
}

void BasicDispatcher::RunUntilIdle() {
  lock_.lock();
  while (!task_queue_.empty()) {
    RunLoopOnce();
  }
  lock_.unlock();
}

void BasicDispatcher::RunUntil(chrono::SystemClock::time_point end_time) {
  lock_.lock();
  while (end_time < now()) {
    RunLoopOnce();
  }
  lock_.unlock();
}

void BasicDispatcher::RunFor(chrono::SystemClock::duration duration) {
  RunUntil(now() + duration);
}

void BasicDispatcher::RunLoopOnce() {
  if (task_queue_.empty() || DueTime(task_queue_.front()) > now()) {
    // Sleep until a notification is received or until the due time of the
    // next task. Notifications are sent when tasks are posted or 'stop' is
    // requested.
    chrono::SystemClock::time_point wake_time =
        task_queue_.empty() ? now() + SLEEP_DURATION
                            : DueTime(task_queue_.front());

    lock_.unlock();
    PW_LOG_DEBUG("no task due; waiting for signal");
    timed_notification_.try_acquire_until(wake_time);
    lock_.lock();

    return;
  }

  while (!task_queue_.empty() && DueTime(task_queue_.front()) <= now()) {
    Task& task = task_queue_.front();
    task_queue_.pop_front();

    if (IsPeriodic(task)) {
      PostTaskInternal(task, DueTime(task) + SetInterval(task));
    }

    lock_.unlock();
    PW_LOG_DEBUG("running task");
    Context ctx{this, &task};
    task(ctx);
    lock_.lock();
  }
}

void BasicDispatcher::RequestStop() {
  std::lock_guard lock(lock_);
  PW_LOG_DEBUG("stop requested");
  stop_requested_ = true;
  task_queue_.clear();
  timed_notification_.release();
}

void BasicDispatcher::PostTask(Task& task) { PostTaskForTime(task, now()); }

void BasicDispatcher::PostDelayedTask(Task& task,
                                      chrono::SystemClock::duration delay) {
  PostTaskForTime(task, now() + delay);
}

void BasicDispatcher::PostTaskForTime(Task& task,
                                      chrono::SystemClock::time_point time) {
  lock_.lock();
  PW_LOG_DEBUG("posting task");
  PostTaskInternal(task, time);
  lock_.unlock();
}

void BasicDispatcher::SchedulePeriodicTask(
    Task& task, chrono::SystemClock::duration interval) {
  SchedulePeriodicTask(task, interval, now());
}

void BasicDispatcher::SchedulePeriodicTask(
    Task& task,
    chrono::SystemClock::duration interval,
    chrono::SystemClock::time_point start_time) {
  SetInterval(task, interval);
  PostTaskForTime(task, start_time);
}

bool BasicDispatcher::Cancel(Task& task) {
  std::lock_guard lock(lock_);
  return task_queue_.remove(task);
}

// Ensure lock_ is held when invoking this function.
void BasicDispatcher::PostTaskInternal(
    Task& task, chrono::SystemClock::time_point time_due) {
  SetDueTime(task, time_due);
  auto it_front = task_queue_.begin();
  auto it_behind = task_queue_.before_begin();
  while (it_front != task_queue_.end() && time_due > DueTime(*it_front)) {
    ++it_front;
    ++it_behind;
  }
  task_queue_.insert_after(it_behind, task);
  timed_notification_.release();
}

}  // namespace pw::async
