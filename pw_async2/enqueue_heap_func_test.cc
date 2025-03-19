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

#include "pw_async2/enqueue_heap_func.h"

#include "pw_async2/dispatcher_base.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::async2::Dispatcher;
using ::pw::async2::EnqueueHeapFunc;

TEST(Dispatcher, DispatcherRunsEnqueuedTasksOnce) {
  Dispatcher dispatcher;
  int ran = 0;
  EnqueueHeapFunc(dispatcher, [&ran]() { ++ran; });
  EXPECT_EQ(ran, 0);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(ran, 1);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(ran, 1);
}

}  // namespace
