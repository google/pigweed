// Copyright 2024 The Pigweed Authors
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

#include <atomic>

#include "gtest/gtest.h"
#include "pw_async2/dispatcher.h"
#include "pw_function/function.h"
#include "pw_thread/sleep.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

namespace pw::async2 {
namespace {

using namespace std::chrono_literals;

class MockTask : public Task {
 public:
  std::atomic_bool should_complete = false;
  int polled = 0;
  int destroyed = 0;
  std::optional<Waker> last_waker = std::nullopt;

 private:
  Poll<> DoPend(Context& cx) override {
    ++polled;
    last_waker = cx.GetWaker(WaitReason::Unspecified());
    if (should_complete) {
      return Ready();
    } else {
      return Pending();
    }
  }
  void DoDestroy() override { ++destroyed; }
};

class FunctionThread : public thread::ThreadCore {
 public:
  explicit FunctionThread(Closure func) : func_(std::move(func)) {}

 private:
  void Run() override { func_(); }

  Closure func_;
};

TEST(Dispatcher, RunToCompletion_SleepsUntilWoken) {
  MockTask task;
  task.should_complete = false;
  Dispatcher dispatcher;
  dispatcher.Post(task);

  FunctionThread delayed_wake([&task]() {
    this_thread::sleep_for(100ms);
    task.should_complete = true;
    std::move(*task.last_waker).Wake();
  });

  thread::Thread work_thread(thread::stl::Options(), delayed_wake);

  dispatcher.RunToCompletion(task);

  work_thread.join();

  // Poll once when sleeping then once when woken.
  EXPECT_EQ(task.polled, 2);
  EXPECT_EQ(task.destroyed, 1);
}

}  // namespace
}  // namespace pw::async2
