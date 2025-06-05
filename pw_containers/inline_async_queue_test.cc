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

#include "pw_containers/inline_async_queue.h"

#include "pw_async2/context.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/try.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::Context;
using pw::async2::Dispatcher;
using pw::async2::PendFuncTask;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;

TEST(InlineAsyncQueueTest, PendZeroReturnsSuccessImmediately) {
  pw::InlineAsyncQueue<int, 4> queue;

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return queue.PendAvailable(context, 0);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncQueueTest, PendWhenAvailableReturnsSuccessImmediately) {
  pw::InlineAsyncQueue<int, 4> queue;
  queue.push(1);
  queue.push(2);

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return queue.PendAvailable(context, 2);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncQueueTest, PendWhenUnavailableWaitsUntilPop) {
  pw::InlineAsyncQueue<int, 4> queue;
  queue.push(1);
  queue.push(2);
  queue.push(3);

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return queue.PendAvailable(context, 3);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  queue.pop();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  queue.pop();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncQueueTest, PendWhenUnavailableWaitsUntilClear) {
  pw::InlineAsyncQueue<int, 4> queue;
  queue.push(1);
  queue.push(2);
  queue.push(3);
  queue.push(4);

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return queue.PendAvailable(context, 2);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  queue.clear();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

}  // namespace
