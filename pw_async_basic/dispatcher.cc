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
#include "pw_async_basic/dispatcher.h"

#include "pw_chrono/system_clock.h"

using namespace std::chrono_literals;

namespace pw::async {

const chrono::SystemClock::duration SLEEP_DURATION = 5s;

BasicDispatcher::~BasicDispatcher() {
  RequestStop();
  lock_.lock();
  DrainTaskQueue();
  lock_.unlock();
}

void BasicDispatcher::Run() {
  lock_.lock();
  while (!stop_requested_) {
    MaybeSleep();
    ExecuteDueTasks();
  }
  DrainTaskQueue();
  lock_.unlock();
}

void BasicDispatcher::RunUntilIdle() {
  lock_.lock();
  ExecuteDueTasks();
  if (stop_requested_) {
    DrainTaskQueue();
  }
  lock_.unlock();
}

void BasicDispatcher::RunUntil(chrono::SystemClock::time_point end_time) {
  lock_.lock();
  while (end_time < now() && !stop_requested_) {
    MaybeSleep();
    ExecuteDueTasks();
  }
  if (stop_requested_) {
    DrainTaskQueue();
  }
  lock_.unlock();
}

void BasicDispatcher::RunFor(chrono::SystemClock::duration duration) {
  RunUntil(now() + duration);
}

void BasicDispatcher::MaybeSleep() {
  if (task_queue_.empty() || task_queue_.front().due_time_ > now()) {
    // Sleep until a notification is received or until the due time of the
    // next task. Notifications are sent when tasks are posted or 'stop' is
    // requested.
    chrono::SystemClock::time_point wake_time =
        task_queue_.empty() ? now() + SLEEP_DURATION
                            : task_queue_.front().due_time_;

    lock_.unlock();
    timed_notification_.try_acquire_until(wake_time);
    lock_.lock();
  }
}

void BasicDispatcher::ExecuteDueTasks() {
  while (!task_queue_.empty() && task_queue_.front().due_time_ <= now() &&
         !stop_requested_) {
    backend::NativeTask& task = task_queue_.front();
    task_queue_.pop_front();

    lock_.unlock();
    Context ctx{this, &task.task_};
    task(ctx, OkStatus());
    lock_.lock();
  }
}

void BasicDispatcher::RequestStop() {
  std::lock_guard lock(lock_);
  stop_requested_ = true;
  timed_notification_.release();
}

void BasicDispatcher::DrainTaskQueue() {
  while (!task_queue_.empty()) {
    backend::NativeTask& task = task_queue_.front();
    task_queue_.pop_front();

    lock_.unlock();
    Context ctx{this, &task.task_};
    task(ctx, Status::Cancelled());
    lock_.lock();
  }
}

void BasicDispatcher::PostAt(Task& task, chrono::SystemClock::time_point time) {
  lock_.lock();
  PostTaskInternal(task.native_type(), time);
  lock_.unlock();
}

bool BasicDispatcher::Cancel(Task& task) {
  std::lock_guard lock(lock_);
  return task_queue_.remove(task.native_type());
}

void BasicDispatcher::PostTaskInternal(
    backend::NativeTask& task, chrono::SystemClock::time_point time_due) {
  task.due_time_ = time_due;
  auto it_front = task_queue_.begin();
  auto it_behind = task_queue_.before_begin();
  while (it_front != task_queue_.end() && time_due > it_front->due_time_) {
    ++it_front;
    ++it_behind;
  }
  task_queue_.insert_after(it_behind, task);
  timed_notification_.release();
}

}  // namespace pw::async
