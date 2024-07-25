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

#include "pw_async2/coro_or_else_task.h"

#include "pw_allocator/null_allocator.h"
#include "pw_allocator/testing.h"
#include "pw_async2/coro.h"
#include "pw_async2/dispatcher_base.h"
#include "pw_status/status.h"

namespace {

using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::allocator::Allocator;
using ::pw::allocator::GetNullAllocator;
using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::Context;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::CoroOrElseTask;
using ::pw::async2::Dispatcher;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;
using ::pw::async2::Waker;

Coro<Result<int>> ImmediatelyReturnsFive(CoroContext&) { co_return 5; }

Coro<Status> StoresFiveThenReturns(CoroContext& coro_cx, int& out) {
  PW_CO_TRY_ASSIGN(out, co_await ImmediatelyReturnsFive(coro_cx));
  co_return OkStatus();
}

TEST(CoroTest, BasicFunctionsWithoutYieldingRun) {
  AllocatorForTest<256> alloc;
  CoroContext coro_cx(alloc);
  int output = 0;
  bool error_handler_did_run = false;
  CoroOrElseTask task(
      StoresFiveThenReturns(coro_cx, output),
      [&error_handler_did_run](Status) { error_handler_did_run = true; });
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(output, 5);
  EXPECT_FALSE(error_handler_did_run);
}

TEST(CoroTest, AllocationFailureProducesInvalidCoro) {
  CoroContext coro_cx(GetNullAllocator());
  EXPECT_FALSE(ImmediatelyReturnsFive(coro_cx).IsValid());
  Status status;
  int output = 0;
  CoroOrElseTask task(StoresFiveThenReturns(coro_cx, output),
                      [&status](Status actual) { status = actual; });
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(status, Status::Internal());
}

}  // namespace
