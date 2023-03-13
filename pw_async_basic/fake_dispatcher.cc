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

#include "pw_async/fake_dispatcher.h"

#include "pw_async/task.h"
#include "pw_log/log.h"

using namespace std::chrono_literals;

namespace pw::async::test::backend {

NativeFakeDispatcher::NativeFakeDispatcher(Dispatcher& dispatcher)
    : dispatcher_(dispatcher) {}

NativeFakeDispatcher::~NativeFakeDispatcher() {
  RequestStop();
  DrainTaskQueue();
}

void NativeFakeDispatcher::RunUntilIdle() {
  ExecuteDueTasks();
  if (stop_requested_) {
    DrainTaskQueue();
  }
}

void NativeFakeDispatcher::RunUntil(chrono::SystemClock::time_point end_time) {
  while (!task_queue_.empty() && task_queue_.front().due_time() <= end_time &&
         !stop_requested_) {
    now_ = task_queue_.front().due_time();
    ExecuteDueTasks();
  }

  if (stop_requested_) {
    DrainTaskQueue();
    return;
  }

  if (now_ < end_time) {
    now_ = end_time;
  }
}

void NativeFakeDispatcher::RunFor(chrono::SystemClock::duration duration) {
  RunUntil(now() + duration);
}

void NativeFakeDispatcher::ExecuteDueTasks() {
  while (!task_queue_.empty() && task_queue_.front().due_time() <= now() &&
         !stop_requested_) {
    ::pw::async::backend::NativeTask& task = task_queue_.front();
    task_queue_.pop_front();

    if (task.interval().has_value()) {
      PostTaskInternal(task, task.due_time() + task.interval().value());
    }

    Context ctx{&dispatcher_, &task.task_};
    task(ctx, OkStatus());
  }
}

void NativeFakeDispatcher::RequestStop() {
  PW_LOG_DEBUG("stop requested");
  stop_requested_ = true;
}

void NativeFakeDispatcher::DrainTaskQueue() {
  while (!task_queue_.empty()) {
    ::pw::async::backend::NativeTask& task = task_queue_.front();
    task_queue_.pop_front();

    PW_LOG_DEBUG("running cancelled task");
    Context ctx{&dispatcher_, &task.task_};
    task(ctx, Status::Cancelled());
  }
}

void NativeFakeDispatcher::PostTask(Task& task) {
  PostTaskForTime(task, now());
}

void NativeFakeDispatcher::PostDelayedTask(
    Task& task, chrono::SystemClock::duration delay) {
  PostTaskForTime(task, now() + delay);
}

void NativeFakeDispatcher::PostTaskForTime(
    Task& task, chrono::SystemClock::time_point time) {
  PW_LOG_DEBUG("posting task");
  PostTaskInternal(task.native_type(), time);
}

void NativeFakeDispatcher::SchedulePeriodicTask(
    Task& task, chrono::SystemClock::duration interval) {
  SchedulePeriodicTask(task, interval, now());
}

void NativeFakeDispatcher::SchedulePeriodicTask(
    Task& task,
    chrono::SystemClock::duration interval,
    chrono::SystemClock::time_point start_time) {
  task.native_type().set_interval(interval);
  PostTaskForTime(task, start_time);
}

bool NativeFakeDispatcher::Cancel(Task& task) {
  return task_queue_.remove(task.native_type());
}

void NativeFakeDispatcher::PostTaskInternal(
    ::pw::async::backend::NativeTask& task,
    chrono::SystemClock::time_point time_due) {
  task.set_due_time(time_due);
  auto it_front = task_queue_.begin();
  auto it_behind = task_queue_.before_begin();
  while (it_front != task_queue_.end() && time_due > it_front->due_time()) {
    ++it_front;
    ++it_behind;
  }
  task_queue_.insert_after(it_behind, task);
}

}  // namespace pw::async::test::backend
