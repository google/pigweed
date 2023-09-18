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

#include "pw_async/heap_dispatcher.h"

#include "pw_async/task.h"
#include "pw_result/result.h"

namespace pw::async {

namespace {

// TODO: b/277793223 - Optimize to avoid double virtual indirection and double
// allocation.  In situations in which pw::Function is large enough and the
// captures are small enough, we could eliminate this by reshaping the task as
// just a pw::Function.
struct TaskAndFunction {
  static Result<TaskAndFunction*> New(TaskFunction&& task) {
    // std::nothrow causes new to return a nullptr on failure instead of
    // throwing.
    TaskAndFunction* t = new (std::nothrow) TaskAndFunction();
    if (!t) {
      return Status::ResourceExhausted();
    }
    t->func = std::move(task);

    // Closure captures must not include references, as that would be UB due to
    // the `delete` at the end of the function. See
    // https://reviews.llvm.org/D48239.
    t->task.set_function([t](Context& ctx, Status status) {
      t->func(ctx, status);

      // Delete must appear at the very end of this closure to avoid
      // use-after-free of captures or Context.task.
      delete t;
    });

    return t;
  }
  Task task;
  TaskFunction func;
};
}  // namespace

Status HeapDispatcher::PostAt(TaskFunction&& task_func,
                              chrono::SystemClock::time_point time) {
  Result<TaskAndFunction*> result = TaskAndFunction::New(std::move(task_func));
  if (!result.ok()) {
    return result.status();
  }
  dispatcher_.PostAt((*result)->task, time);
  return Status();
}

}  // namespace pw::async
