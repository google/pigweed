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

#include "pw_async2/pend_func_awaitable.h"

#include <optional>
#include <utility>

#include "pw_allocator/testing.h"
#include "pw_async2/coro.h"
#include "pw_async2/coro_or_else_task.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::Function;
using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::Context;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::CoroOrElseTask;
using ::pw::async2::Dispatcher;
using ::pw::async2::PendFuncAwaitable;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::PollResult;
using ::pw::async2::Ready;
using ::pw::async2::Waker;

template <typename T>
class Mailbox {
 public:
  PollResult<T> PendGetValue(Context& cx) {
    ++poll_count_;
    if (value_) {
      auto v = *value_;
      value_.reset();
      return Ready(v);
    }
    PW_ASYNC_STORE_WAKER(cx, waker_, "Mailbox is waiting for a value");
    return Pending();
  }

  void SetValue(T v) {
    value_ = v;
    std::move(waker_).Wake();
  }

  int PollCount() { return poll_count_; }

 private:
  std::optional<T> value_;
  int poll_count_ = 0;
  Waker waker_;
};

template <typename T>
Coro<Status> ReadMailbox(CoroContext&, Mailbox<T>& mailbox, int& out) {
  PW_CO_TRY_ASSIGN(out, co_await PendFuncAwaitable([&](Context& cx) {
                     return mailbox.PendGetValue(cx);
                   }));
  co_return OkStatus();
}

TEST(PendFuncAwaitable, TestMailbox) {
  Mailbox<int> mailbox;

  AllocatorForTest<256> alloc;
  CoroContext coro_cx(alloc);
  int output = 0;
  bool error_handler_did_run = false;
  CoroOrElseTask task(
      ReadMailbox(coro_cx, mailbox, output),
      [&error_handler_did_run](Status) { error_handler_did_run = true; });

  Dispatcher dispatcher;
  dispatcher.Post(task);

  EXPECT_EQ(mailbox.PollCount(), 0);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(mailbox.PollCount(), 1);

  // Unwoken mailbox is not polled.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(mailbox.PollCount(), 1);

  mailbox.SetValue(5);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(mailbox.PollCount(), 2);
  EXPECT_EQ(output, 5);
  EXPECT_FALSE(error_handler_did_run);
}

Poll<int> ReturnsReady8(Context&) { return Ready(8); }

TEST(PendFuncAwaitable, TestTemplateDeductionAndSize) {
  // A PendFuncAwaitable with an unspecified Func template parameter will
  // default to pw::Function. This allows the same container to hold a variety
  // of different callables, but it may either reserve extra inline storage or
  // dynamically allocate momeory, depending on how pw::Function is configured.
  std::optional<PendFuncAwaitable<int>> a;
  a.emplace([](Context&) -> Poll<int> { return Ready(4); });
  a.emplace(&ReturnsReady8);
  static_assert(sizeof(decltype(a)::value_type::CallableType) ==
                sizeof(Function<Poll<int>(Context&)>));

  // When constructing a PendFuncAwaitable directly from a callable, CTAD will
  // match the Func template parameter to that of the callable. This has the
  // benefit of reducing the amount of storage needed vs that of a pw::Function.
  //
  // A lambda without any captures doesn't require any storage.
  auto b = PendFuncAwaitable([](Context&) -> Poll<int> { return Ready(4); });
  static_assert(sizeof(decltype(b)::CallableType) <= 1);

  // A lambda with captures requires storage to hold the captures.
  int scratch = 6;
  auto c = PendFuncAwaitable(
      [&scratch](Context&) -> Poll<int> { return Ready(scratch); });
  static_assert(sizeof(decltype(c)::CallableType) == sizeof(&scratch));

  // A raw function pointer just needs storage for the pointer value.
  auto d = PendFuncAwaitable(&ReturnsReady8);
  static_assert(sizeof(decltype(d)::CallableType) == sizeof(&ReturnsReady8));
}

}  // namespace
