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

#include "pw_allocator/allocator.h"
#include "pw_async2/dispatcher.h"
#include "pw_status/status.h"

namespace {

using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::async2::Context;
using ::pw::async2::Poll;

class MyData {
 public:
};

class ReceiveFuture {
 public:
  Poll<Result<MyData>> Pend(Context&) { return MyData(); }
};

class MyReceiver {
 public:
  ReceiveFuture Receive() { return ReceiveFuture(); }
  Poll<Result<MyData>> PendReceive(Context&) { return MyData(); }
};

class SendFuture {
 public:
  Poll<Status> Pend(Context&) { return OkStatus(); }
};

class MySender {
 public:
  SendFuture Send(MyData&&) { return SendFuture(); }
};

}  // namespace

// NOTE: we double-include so that the example shows the `#includes`, but we
// can still define types beforehand.

// DOCSTAG: [pw_async2-examples-basic-manual]
#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"
#include "pw_async2/try.h"
#include "pw_log/log.h"
#include "pw_result/result.h"

namespace {

using ::pw::async2::Context;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;

class MyReceiver;
class MySender;

// Receive then send that data asynchronously. If the reader or writer aren't
// ready, the task suspends when TRY_READY invokes ``return Pending()``.
class ReceiveAndSend final : public Task {
 public:
  ReceiveAndSend(MyReceiver receiver, MySender sender)
      : receiver_(receiver), sender_(sender), state_(kReceiving) {}

  Poll<> DoPend(Context& cx) final {
    switch (state_) {
      case kReceiving: {
        PW_TRY_READY_ASSIGN(auto new_data, receiver_.PendReceive(cx));
        if (!new_data.ok()) {
          PW_LOG_ERROR("Receiving failed: %s", new_data.status().str());
          return Ready();  // Completes the task.
        }
        // Start transmitting and switch to transmitting state.
        send_future_ = sender_.Send(std::move(*new_data));
        state_ = kTransmitting;
      }
        [[fallthrough]];
      case kTransmitting: {
        PW_TRY_READY_ASSIGN(auto sent, send_future_->Pend(cx));
        if (!sent.ok()) {
          PW_LOG_ERROR("Sending failed: %s", sent.str());
        }
        return Ready();  // Completes the task.
      }
    }
  }

 private:
  MyReceiver receiver_;  // Can receive data async.
  MySender sender_;      // Can send data async.
  std::optional<SendFuture> send_future_ = std::nullopt;

  enum State { kTransmitting, kReceiving };
  State state_;
};

}  // namespace
// DOCSTAG: [pw_async2-examples-basic-manual]

// DOCSTAG: [pw_async2-examples-basic-coro]
#include "pw_allocator/allocator.h"
#include "pw_async2/coro.h"
#include "pw_log/log.h"
#include "pw_result/result.h"

namespace {

using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;

class MyReceiver;
class MySender;

/// Create a coroutine which asynchronously receives a value from
/// ``receiver`` and forwards it to ``sender``.
///
/// Note: the ``CoroContext`` argument is used by the ``Coro<T>`` internals to
/// allocate the coroutine state. If this allocation fails, ``Coro<Status>``
/// will return ``Status::Internal()``.
Coro<Status> ReceiveAndSendCoro(CoroContext&,
                                MyReceiver receiver,
                                MySender sender) {
  Result<MyData> data = co_await receiver.Receive();
  if (!data.ok()) {
    PW_LOG_ERROR("Receiving failed: %s", data.status().str());
    co_return Status::Unavailable();
  }
  Status sent = co_await sender.Send(std::move(*data));
  if (!sent.ok()) {
    PW_LOG_ERROR("Sending failed: %s", sent.str());
    co_return Status::Unavailable();
  }
  co_return OkStatus();
}

}  // namespace
// DOCSTAG: [pw_async2-examples-basic-coro]

#include "pw_allocator/testing.h"

namespace {

using ::pw::OkStatus;
using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::Context;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::Dispatcher;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;

TEST(ManualExample, ReturnsOk) {
  auto task = ReceiveAndSend(MyReceiver(), MySender());
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

TEST(ManualExample, Runs) {
  auto receiver = MyReceiver();
  auto sender = MySender();
  // DOCSTAG: [pw_async2-examples-basic-dispatcher]
  auto task = ReceiveAndSend(std::move(receiver), std::move(sender));
  Dispatcher dispatcher;
  // Registers `task` to run on the dispatcher.
  dispatcher.Post(task);
  // Sets the dispatcher to run until all `Post`ed tasks have completed.
  dispatcher.RunToCompletion();
  // DOCSTAG: [pw_async2-examples-basic-dispatcher]
}

TEST(CoroExample, ReturnsOk) {
  AllocatorForTest<256> alloc;
  CoroContext coro_cx(alloc);
  auto coro = ReceiveAndSendCoro(coro_cx, MyReceiver(), MySender());
  Dispatcher dispatcher;
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(coro), Ready(OkStatus()));
}

}  // namespace
