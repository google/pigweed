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

#include "pw_async2/dispatcher.h"

namespace pw::async2 {
namespace internal {

template <typename Func>
class RunHeapFuncTask : public Task {
 public:
  static Task& New(Func&& func) {
    return *(new RunHeapFuncTask(std::forward<Func>(func)));
  }

 private:
  RunHeapFuncTask(Func&& func) : func_(std::forward<Func>(func)) {}
  Poll<> DoPend(Context&) final {
    func_();
    return Ready();
  }
  void DoDestroy() final { delete this; }
  Func func_;
};

}  // namespace internal

/// Heap-allocates space for ``func`` and enqueues it to run on ``dispatcher``.
///
/// ``func`` must be a no-argument callable that returns ``void``.
///
/// This function requires heap allocation using ``new`` be available.
template <typename Func>
void EnqueueHeapFunc(Dispatcher& dispatcher, Func&& func) {
  return dispatcher.Post(
      internal::RunHeapFuncTask<Func>::New(std::forward<Func>(func)));
}

}  // namespace pw::async2
