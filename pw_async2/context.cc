// Copyright 2025 The Pigweed Authors
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

#include "pw_async2/context.h"

#include <mutex>

namespace pw::async2 {

void Context::ReEnqueue() {
  Waker waker;
  // The new waker will be immediately woken and removed, so its wait reason
  // does not matter.
  internal::CloneWaker(*waker_, waker);
  std::move(waker).Wake();
}

}  // namespace pw::async2
