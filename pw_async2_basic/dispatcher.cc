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

#include <mutex>
#include <optional>

#include "pw_assert/check.h"
#include "pw_async2/dispatcher_native.h"
#include "pw_chrono/system_clock.h"

namespace pw::async2 {

Poll<> Dispatcher::DoRunUntilStalled(Task* task) {
  {
    std::lock_guard lock(dispatcher_lock());
    PW_CHECK(task == nullptr || HasPostedTask(*task),
             "Attempted to run a dispatcher until a task was stalled, "
             "but that task has not been `Post`ed to that `Dispatcher`.");
  }
  while (true) {
    RunOneTaskResult result = RunOneTask(task);
    if (result.completed_main_task() || result.completed_all_tasks()) {
      return Ready();
    }
    if (!result.ran_a_task()) {
      return Pending();
    }
  }
}

void Dispatcher::DoRunToCompletion(Task* task) {
  {
    std::lock_guard lock(dispatcher_lock());
    PW_CHECK(task == nullptr || HasPostedTask(*task),
             "Attempted to run a dispatcher until a task was complete, "
             "but that task has not been `Post`ed to that `Dispatcher`.");
  }
  while (true) {
    RunOneTaskResult result = RunOneTask(task);
    if (result.completed_main_task() || result.completed_all_tasks()) {
      return;
    }
    if (!result.ran_a_task()) {
      SleepInfo sleep_info = AttemptRequestWake();
      if (sleep_info.should_sleep()) {
        notify_.acquire();
      }
    }
  }
}

}  // namespace pw::async2
