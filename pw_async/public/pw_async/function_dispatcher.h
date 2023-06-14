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
#pragma once

#include <utility>

#include "pw_async/dispatcher.h"
#include "pw_async/task_function.h"
#include "pw_status/status.h"

namespace pw::async {

/// FunctionDispatcher extends Dispatcher with Post*() methods that take a
/// TaskFunction instead of a Task. This implies that Tasks are allocated or
/// are taken from a Task pool. Tasks are owned and managed by the Dispatcher.
class FunctionDispatcher : public Dispatcher {
 public:
  ~FunctionDispatcher() override = default;

  // Prevent hiding of overloaded virtual methods.
  using Dispatcher::Post;
  using Dispatcher::PostAfter;
  using Dispatcher::PostAt;

  /// Post dispatcher owned |task_func| function.
  virtual Status Post(TaskFunction&& task_func) {
    return PostAt(std::move(task_func), now());
  }

  /// Post dispatcher owned |task_func| function to be run after |delay|.
  virtual Status PostAfter(TaskFunction&& task_func,
                           chrono::SystemClock::duration delay) {
    return PostAt(std::move(task_func), now() + delay);
  }

  /// Post dispatcher owned |task_func| function to be run at |time|.
  virtual Status PostAt(TaskFunction&& task_func,
                        chrono::SystemClock::time_point time) = 0;
};

}  // namespace pw::async
