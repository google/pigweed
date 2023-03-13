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
#pragma once

#include "pw_function/function.h"
#include "pw_status/status.h"

namespace pw::async {

class Dispatcher;
class Task;

struct Context {
  Dispatcher* dispatcher;
  Task* task;
};

// A TaskFunction is a unit of work that is wrapped by a Task and executed on a
// Dispatcher.
//
// TaskFunctions take a `Context` as their first argument. Before executing a
// Task, the Dispatcher sets the pointer to itself and to the Task in `Context`.
//
// TaskFunctions take a `Status` as their second argument. When a Task is
// running as normal, |status| == PW_STATUS_OK. If a Task will not be able to
// run as scheduled, the Dispatcher will still invoke the TaskFunction with
// |status| == PW_STATUS_CANCELLED. This provides an opportunity to reclaim
// resources held by the Task.
//
// A Task will not run as scheduled if, for example, it is still waiting when
// the Dispatcher shuts down.
using TaskFunction = Function<void(Context&, Status)>;

}  // namespace pw::async
