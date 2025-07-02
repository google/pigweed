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

#include "pw_containers/inline_async_deque.h"

#include "pw_async2/context.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/try.h"
#include "pw_containers/internal/container_tests.h"
#include "pw_containers/internal/test_helpers.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::Context;
using pw::async2::Dispatcher;
using pw::async2::PendFuncTask;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;

static_assert(!std::is_constructible_v<pw::InlineAsyncDeque<int>>,
              "Cannot construct generic capacity container");

// Instantiate shared deque tests.
template <size_t kCapacity>
class CommonTest
    : public ::pw::containers::test::CommonTestFixture<CommonTest<kCapacity>> {
 public:
  template <typename T>
  class Container : public pw::InlineAsyncDeque<T, kCapacity> {
   public:
    Container(CommonTest&) {}
  };
};

using InlineAsyncDequeCommonTest9 = CommonTest<9>;
using InlineAsyncDequeCommonTest16 = CommonTest<16>;

PW_CONTAINERS_COMMON_DEQUE_TESTS(InlineAsyncDequeCommonTest9);
PW_CONTAINERS_COMMON_DEQUE_TESTS(InlineAsyncDequeCommonTest16);

TEST(InlineAsyncDequeTest, PendHasZeroSpaceReturnsSuccessImmediately) {
  pw::InlineAsyncDeque<int, 4> deque;

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 0);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendHasSpaceWhenAvailableReturnsSuccessImmediately) {
  pw::InlineAsyncDeque<int, 4> deque;
  deque.push_back(1);
  deque.push_back(2);

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 2);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendHasSpaceWhenFullWaitsUntilPopFront) {
  pw::InlineAsyncDeque<int, 4> deque;
  deque.push_back(1);
  deque.push_back(2);
  deque.push_back(3);

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 3);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.pop_front();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.pop_front();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendHasSpaceWhenFullWaitsUntilPopBack) {
  pw::InlineAsyncDeque<int, 4> deque;
  deque.push_back(1);
  deque.push_back(2);
  deque.push_back(3);

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 3);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.pop_back();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.pop_back();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendHasSpaceWhenFullWaitsUntilClear) {
  pw::InlineAsyncDeque<int, 4> deque;
  deque.push_back(1);
  deque.push_back(2);
  deque.push_back(3);
  deque.push_back(4);

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 2);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.clear();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendHasSpaceOnGenericSizedReference) {
  pw::InlineAsyncDeque<int, 4> deque1;
  pw::InlineAsyncDeque<int>& deque2 = deque1;

  Dispatcher dispatcher;
  PendFuncTask task([&](Context& context) -> Poll<> {
    return deque2.PendHasSpace(context, 1);
  });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendHasSpaceWaitsAfterReadyUntilPushFront) {
  pw::InlineAsyncDeque<int, 4> deque;
  Dispatcher dispatcher;

  PendFuncTask task1([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 1);
  });
  dispatcher.Post(task1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());

  PendFuncTask task2([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 2);
  });
  dispatcher.Post(task2);

  // Even though there is room, the queue returns "Pending" until the space
  // reserved by the first task has been claimed.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.push_front(1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendHasSpaceWaitsAfterReadyUntilPushBack) {
  pw::InlineAsyncDeque<int, 4> deque;
  Dispatcher dispatcher;

  PendFuncTask task1([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 1);
  });
  dispatcher.Post(task1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());

  PendFuncTask task2([&](Context& context) -> Poll<> {
    return deque.PendHasSpace(context, 2);
  });
  dispatcher.Post(task2);

  // Even though there is room, the queue returns "Pending" until the space
  // reserved by the first task has been claimed.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.push_back(1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendNotEmptyWhenNotEmptyReturnsSuccessImmediately) {
  pw::InlineAsyncDeque<int, 4> deque;
  deque.push_back(1);

  Dispatcher dispatcher;
  PendFuncTask task(
      [&](Context& context) -> Poll<> { return deque.PendNotEmpty(context); });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendNotEmptyWhenEmptyWaitsUntilPush) {
  pw::InlineAsyncDeque<int, 4> deque;

  Dispatcher dispatcher;
  PendFuncTask task(
      [&](Context& context) -> Poll<> { return deque.PendNotEmpty(context); });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.push_back(1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendNotEmptyOnGenericSizedReference) {
  pw::InlineAsyncDeque<int, 4> deque1;
  pw::InlineAsyncDeque<int>& deque2 = deque1;
  deque2.push_back(1);

  Dispatcher dispatcher;
  PendFuncTask task(
      [&](Context& context) -> Poll<> { return deque2.PendNotEmpty(context); });
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendNotEmptyWaitsAfterReadyUntilPopFront) {
  pw::InlineAsyncDeque<int, 4> deque;
  Dispatcher dispatcher;
  deque.push_back(1);
  deque.push_back(2);

  PendFuncTask task1(
      [&](Context& context) -> Poll<> { return deque.PendNotEmpty(context); });
  dispatcher.Post(task1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());

  PendFuncTask task2(
      [&](Context& context) -> Poll<> { return deque.PendNotEmpty(context); });
  dispatcher.Post(task2);

  // Even though there is an item, the queue returns "Pending" until the item
  // reserved by the first task has been claimed.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.pop_front();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(InlineAsyncDequeTest, PendNotEmptyWaitsAfterReadyUntilPopBack) {
  pw::InlineAsyncDeque<int, 4> deque;
  Dispatcher dispatcher;
  deque.push_back(1);
  deque.push_back(2);

  PendFuncTask task1(
      [&](Context& context) -> Poll<> { return deque.PendNotEmpty(context); });
  dispatcher.Post(task1);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());

  PendFuncTask task2(
      [&](Context& context) -> Poll<> { return deque.PendNotEmpty(context); });
  dispatcher.Post(task2);

  // Even though there is an item, the queue returns "Pending" until the item
  // reserved by the first task has been claimed.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  deque.pop_back();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

}  // namespace
