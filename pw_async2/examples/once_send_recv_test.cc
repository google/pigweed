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

#define PW_LOG_MODULE_NAME "EXAMPLES_ONCE_SEND_RECV"

#include "pw_allocator/libc_allocator.h"
#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_async2/context.h"
#include "pw_async2/coro.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/once_sender.h"
#include "pw_async2/poll.h"
#include "pw_async2/try.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_unit_test/framework.h"

namespace examples::manual {

using ::pw::Result;
using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::MakeOnceSenderAndReceiver;
using ::pw::async2::OnceReceiver;
using ::pw::async2::Poll;
using ::pw::async2::PollResult;
using ::pw::async2::Ready;
using ::pw::async2::Task;

class ReceiveAndLogValueTask : public Task {
 public:
  // The receiver should take ownership of an appropriate OnceReceiver<T>.
  explicit ReceiveAndLogValueTask(OnceReceiver<int>&& int_receiver)
      : int_receiver_(std::move(int_receiver)) {}

 private:
  Poll<> DoPend(Context& cx) override {
    // DOCSTAG: [pw_async2-examples-once-send-recv-receiving]
    PW_TRY_READY_ASSIGN(Result<int> value, int_receiver_.Pend(cx));
    if (!value.ok()) {
      PW_LOG_ERROR(
          "OnceSender was destroyed without sending a message! Outrageous :(");
    }
    PW_LOG_INFO("Received the integer value: %d", *value);
    // DOCSTAG: [pw_async2-examples-once-send-recv-receiving]
    return Ready();
  }

  OnceReceiver<int> int_receiver_;
};

int main() {
  // DOCSTAG: [pw_async2-examples-once-send-recv-construction]
  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  ReceiveAndLogValueTask task(std::move(receiver));
  // DOCSTAG: [pw_async2-examples-once-send-recv-construction]
  Dispatcher dispatcher;
  dispatcher.Post(task);

  // DOCSTAG: [pw_async2-examples-once-send-recv-send-value]
  // Send a value to the task
  sender.emplace(5);
  // DOCSTAG: [pw_async2-examples-once-send-recv-send-value]

  dispatcher.RunToCompletion();

  return 0;
}

}  // namespace examples::manual

namespace examples::coro {

using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::allocator::LibCAllocator;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::Dispatcher;
using ::pw::async2::MakeOnceSenderAndReceiver;
using ::pw::async2::OnceReceiver;
using ::pw::async2::Ready;

// The receiver should take ownership of an appropriate OnceReceiver<T>.
Coro<Status> ReceiveAndLogValue(CoroContext&, OnceReceiver<int> int_receiver) {
  // DOCSTAG: [pw_async2-examples-once-send-recv-coro-await]
  Result<int> value = co_await int_receiver;
  if (!value.ok()) {
    PW_LOG_ERROR(
        "OnceSender was destroyed without sending a message! Outrageous :(");
    co_return Status::Cancelled();
  }
  PW_LOG_INFO("Got an int: %d", *value);
  // DOCSTAG: [pw_async2-examples-once-send-recv-coro-await]

  co_return OkStatus();
}

int main() {
  LibCAllocator alloc;
  CoroContext coro_cx(alloc);
  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  auto coro = ReceiveAndLogValue(coro_cx, std::move(receiver));

  Dispatcher dispatcher;
  PW_CHECK(dispatcher.RunPendableUntilStalled(coro).IsPending());

  // Send a value to the task
  sender.emplace(5);

  PW_CHECK(dispatcher.RunPendableUntilStalled(coro) == Ready(OkStatus()));
  return 0;
}

}  // namespace examples::coro

namespace {

using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::Context;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::Dispatcher;
using ::pw::async2::MakeOnceSenderAndReceiver;
using ::pw::async2::OnceReceiver;
using ::pw::async2::Poll;
using ::pw::async2::PollResult;
using ::pw::async2::Ready;
using ::pw::async2::Task;

TEST(OnceSendRecv, ReceiveAndLogValueTask) {
  // Ensure the example code runs.
  examples::manual::main();

  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  examples::manual::ReceiveAndLogValueTask task(std::move(receiver));
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());
  sender.emplace(5);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

TEST(OnceSendRecv, ReceiveAndLogValueCoro) {
  // Ensure the example code runs.
  examples::coro::main();

  AllocatorForTest<256> alloc;
  CoroContext coro_cx(alloc);
  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  auto coro = examples::coro::ReceiveAndLogValue(coro_cx, std::move(receiver));
  Dispatcher dispatcher;
  EXPECT_TRUE(dispatcher.RunPendableUntilStalled(coro).IsPending());
  sender.emplace(5);
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(coro), Ready(OkStatus()));
}

}  // namespace
