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

namespace pw::async {

class Dispatcher;
class Task;

// Task functions take a `Context` as their argument. Before executing a Task,
// the Dispatcher sets the pointer to itself and to the Task in `Context`.
struct Context {
  Dispatcher* dispatcher;
  Task* task;
};

using TaskFunction = Function<void(Context&)>;

}  // namespace pw::async
