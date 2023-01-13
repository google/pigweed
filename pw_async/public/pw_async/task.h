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

#include <array>

#include "pw_chrono/system_clock.h"
#include "pw_containers/intrusive_list.h"
#include "pw_function/function.h"

namespace pw::async {

constexpr size_t kTaskStateBytes = sizeof(void*) * 4;

class Dispatcher;
class Task;

// Task functions take a `Context` as their argument. Before executing a Task,
// the Dispatcher sets the pointer to itself and to the Task in `Context`.
struct Context {
  Dispatcher* dispatcher;
  Task* task;
};

// TODO(saeedali): Remove IntrusiveList from here.
class Task : public IntrusiveList<Task>::Item {
 public:
  using TaskFunction = Function<void(Context&)>;

  Task() { state_.fill(static_cast<std::byte>(0)); }
  explicit Task(TaskFunction&& f) {
    state_.fill(static_cast<std::byte>(0));
    SetFunction(std::move(f));
  }

  void SetFunction(TaskFunction&& f) { f_ = std::move(f); }

  void operator()(Context& ctx) { f_(ctx); }

  std::byte* State() { return state_.data(); }

 private:
  // Dispatchers use `state_` to store per Task information. Unused space can be
  // used by clients to store custom information in their Tasks.
  std::array<std::byte, kTaskStateBytes> state_;
  TaskFunction f_;
};

}  // namespace pw::async
