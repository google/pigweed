// Copyright 2021 The Pigweed Authors
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

#include "RTOS.h"
#include "pw_function/function.h"
#include "pw_status/status.h"

namespace pw::thread::embos {

// A callback that is executed for each thread when using ForEachThread().
using ThreadCallback = pw::Function<Status(const OS_TASK&)>;

// Iterates through all threads that haven't been deleted, calling the provided
// callback on each thread. If the callback fails on one thread, the iteration
// stops.
//
// Precondition:
//   OS_Start() must be called prior to using this function.
//
// Returns:
//   FailedPrecondition - The scheduler has not yet been initialized.
//   OkStatus - Successfully iterated over all threads.
//   Other statuses may be returned by the ThreadCallback.
//
// Warning: This is only safe to use when the scheduler is disabled.
Status ForEachThread(ThreadCallback& cb);

namespace internal {

// This function is exposed for testing. Prefer
// pw::thread::embos::ForEachThread.
Status ForEachThread(const OS_TASK& starting_thread, ThreadCallback& cb);

}  // namespace internal
}  // namespace pw::thread::embos
