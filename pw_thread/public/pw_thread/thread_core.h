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

namespace pw::thread {

// An optional virtual interface which can be implemented by objects which are
// a thread as a helper to use pw::thread::Thread.
//
// ThreadCore was originally intended to avoid indirection when threads were
// constructed from a function pointer and void* argument. Since Thread now uses
// pw::Function<void()>, ThreadCore is no longer necessary. Its use is
// discouraged in new code.
//
// WARNING: Because the thread may start after the pw::Thread creation, an
// object which implements the ThreadCore MUST meet or exceed the lifetime of
// its thread of execution!
class ThreadCore {
 public:
  virtual ~ThreadCore() = default;

  // The public API to start a ThreadCore, note that this may return.
  void Start() { Run(); }

 private:
  // This function may return.
  virtual void Run() = 0;
};

}  // namespace pw::thread
