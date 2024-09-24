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

// DOCSTAG: [pw_async2-examples-once-send-recv-manual]
#include "pw_async2/dispatcher.h"
#include "pw_async2/dispatcher_native.h"
#include "pw_async2/once_sender.h"
#include "pw_async2/poll.h"
#include "pw_log/log.h"
#include "pw_result/result.h"

namespace {

using ::pw::Result;
using ::pw::async2::Context;
using ::pw::async2::OnceReceiver;
using ::pw::async2::OnceSender;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;

class ReceiveAndLogValueTask : public Task {
 public:
  ReceiveAndLogValueTask(OnceReceiver<int>&& int_receiver)
      : int_receiver_(std::move(int_receiver)) {}

 private:
  Poll<> DoPend(Context& cx) override {
    Poll<Result<int>> value = int_receiver_.Pend(cx);
    if (value.IsPending()) {
      return Pending();
    }
    if (!value->ok()) {
      PW_LOG_ERROR(
          "OnceSender was destroyed without sending a message! Outrageous :(");
    }
    PW_LOG_INFO("Got an int: %d", **value);
    return Ready();
  }

  OnceReceiver<int> int_receiver_;
};

}  // namespace
// DOCSTAG: [pw_async2-examples-once-send-recv-manual]

// DOCSTAG: [pw_async2-examples-once-send-recv-coro]
#include "pw_async2/coro.h"
#include "pw_async2/once_sender.h"
#include "pw_log/log.h"
#include "pw_result/result.h"

namespace {

using ::pw::OkStatus;
using ::pw::Status;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::OnceReceiver;
using ::pw::async2::OnceSender;

Coro<Status> ReceiveAndLogValue(CoroContext&, OnceReceiver<int> int_receiver) {
  Result<int> value = co_await int_receiver;
  if (!value.ok()) {
    PW_LOG_ERROR(
        "OnceSender was destroyed without sending a message! Outrageous :(");
    co_return Status::Cancelled();
  }
  PW_LOG_INFO("Got an int: %d", *value);
  co_return OkStatus();
}

}  // namespace
// DOCSTAG: [pw_async2-examples-once-send-recv-coro]

#include "pw_allocator/testing.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::Dispatcher;
using ::pw::async2::MakeOnceSenderAndReceiver;

TEST(OnceSendRecv, ReceiveAndLogValueTask) {
  auto send_recv = MakeOnceSenderAndReceiver<int>();
  ReceiveAndLogValueTask task(std::move(send_recv.second));
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());
  send_recv.first.emplace(5);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

class ExpectCoroTask final : public Task {
 public:
  ExpectCoroTask(Coro<pw::Status>&& coro) : coro_(std::move(coro)) {}

 private:
  Poll<> DoPend(Context& cx) final {
    Poll<Status> result = coro_.Pend(cx);
    if (result.IsPending()) {
      return Pending();
    }
    EXPECT_EQ(*result, OkStatus());
    return Ready();
  }
  Coro<pw::Status> coro_;
};

TEST(OnceSendRecv, ReceiveAndLogValueCoro) {
  AllocatorForTest<256> alloc;
  CoroContext coro_cx(alloc);
  auto send_recv = MakeOnceSenderAndReceiver<int>();
  ExpectCoroTask task(ReceiveAndLogValue(coro_cx, std::move(send_recv.second)));
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());
  send_recv.first.emplace(5);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

}  // namespace
