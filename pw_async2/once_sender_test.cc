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

#include "pw_async2/once_sender.h"

#include "pw_async2/dispatcher.h"
#include "pw_containers/vector.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::OnceReceiver;
using ::pw::async2::OnceRefReceiver;
using ::pw::async2::OnceRefSender;
using ::pw::async2::OnceSender;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;

class MoveOnlyValue {
 public:
  MoveOnlyValue(int value) : value_(value) {}
  MoveOnlyValue(const MoveOnlyValue&) = delete;
  MoveOnlyValue& operator=(const MoveOnlyValue&) = delete;
  MoveOnlyValue(MoveOnlyValue&&) = default;
  MoveOnlyValue& operator=(MoveOnlyValue&&) = default;

  int& value() { return value_; }

 private:
  int value_;
};

class ValueTask : public Task {
 public:
  ValueTask(bool use_make_constructor = true)
      : use_make_constructor_(use_make_constructor) {}
  Poll<> DoPend(Context& cx) override {
    if (!receiver_) {
      if (use_make_constructor_) {
        auto [send, recv] =
            pw::async2::MakeOnceSenderAndReceiver<MoveOnlyValue>();
        sender_.emplace(std::move(send));
        receiver_.emplace(std::move(recv));
      } else {
        receiver_.emplace();
        sender_.emplace();
        pw::async2::InitializeOnceSenderAndReceiver<MoveOnlyValue>(
            sender_.value(), receiver_.value());
      }
    }
    Poll<pw::Result<MoveOnlyValue>> poll = receiver_.value().Pend(cx);
    if (poll.IsReady()) {
      ready_value_.emplace(std::move(poll.value()));
      return Ready();
    }
    return Pending();
  }
  std::optional<pw::Result<MoveOnlyValue>>& ready_value() {
    return ready_value_;
  }
  std::optional<OnceSender<MoveOnlyValue>>& sender() { return sender_; }

  void DestroySender() { sender_.reset(); }
  void DestroyReceiver() { receiver_.reset(); }

 private:
  bool use_make_constructor_;
  std::optional<pw::Result<MoveOnlyValue>> ready_value_;
  std::optional<OnceReceiver<MoveOnlyValue>> receiver_;
  std::optional<OnceSender<MoveOnlyValue>> sender_;
};

TEST(OnceSender, OnceSenderEmplace) {
  Dispatcher dispatcher;
  ValueTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.sender()->emplace(5);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());

  ASSERT_TRUE(task.ready_value().has_value());
  ASSERT_TRUE(task.ready_value().value().ok());
  EXPECT_EQ(task.ready_value().value()->value(), 5);
}

TEST(OnceSender, OnceSenderEmplaceUseInitializeConstructor) {
  Dispatcher dispatcher;
  ValueTask task(/*use_make_constructor=*/false);
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.sender()->emplace(5);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());

  ASSERT_TRUE(task.ready_value().has_value());
  ASSERT_TRUE(task.ready_value().value().ok());
  EXPECT_EQ(task.ready_value().value()->value(), 5);
}

TEST(OnceSender, OnceSenderMoveAssign) {
  Dispatcher dispatcher;
  ValueTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  MoveOnlyValue value(7);
  *task.sender() = std::move(value);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());

  ASSERT_TRUE(task.ready_value().has_value());
  ASSERT_TRUE(task.ready_value().value().ok());
  EXPECT_EQ(task.ready_value().value()->value(), 7);
}

TEST(OnceSender, DestroyingOnceSenderCausesReceiverPendToReturnCancelled) {
  Dispatcher dispatcher;
  ValueTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.DestroySender();
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  task.DestroyReceiver();

  ASSERT_TRUE(task.ready_value().has_value());
  ASSERT_FALSE(task.ready_value().value().ok());
  ASSERT_TRUE(task.ready_value().value().status().IsCancelled());
}

TEST(OnceSender, DestroyingOnceReceiverCausesSenderMethodsToBeNoOps) {
  Dispatcher dispatcher;
  ValueTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.DestroyReceiver();
  task.sender()->emplace(6);
  task.DestroySender();
  task.Deregister();
}

class VectorTask : public Task {
 public:
  VectorTask(bool use_make_constructor = true)
      : use_make_constructor_(use_make_constructor) {}

  Poll<> DoPend(Context& cx) override {
    if (!receiver_) {
      if (use_make_constructor_) {
        auto [send, recv] =
            pw::async2::MakeOnceRefSenderAndReceiver<pw::Vector<int>>(value_);
        sender_.emplace(std::move(send));
        receiver_.emplace(std::move(recv));
      } else {
        sender_.emplace();
        receiver_.emplace();
        pw::async2::InitializeOnceRefSenderAndReceiver<pw::Vector<int>>(
            sender_.value(), receiver_.value(), value_);
      }
    }
    Poll<pw::Status> poll = receiver_->Pend(cx);
    if (poll.IsReady()) {
      ready_value_.emplace(poll.value());
      return Ready();
    }
    return Pending();
  }

  const pw::Vector<int>& value() const { return value_; }
  const std::optional<pw::Status>& ready_value() const { return ready_value_; }
  std::optional<OnceRefSender<pw::Vector<int>>>& sender() { return sender_; }

  void DestroySender() { sender_.reset(); }
  void DestroyReceiver() { receiver_.reset(); }

 private:
  bool use_make_constructor_;
  pw::Vector<int, 3> value_;
  std::optional<pw::Status> ready_value_;
  std::optional<OnceRefReceiver<pw::Vector<int>>> receiver_;
  std::optional<OnceRefSender<pw::Vector<int>>> sender_;
};

TEST(OnceSender, OnceRefSenderSetConstRef) {
  Dispatcher dispatcher;
  VectorTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  pw::Vector<int, 2> other = {0, 1};
  task.sender()->Set(other);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.value()[0], 0);
  EXPECT_EQ(task.value()[1], 1);
}

TEST(OnceSender, OnceRefSenderSetConstRefUseInitializeConstructor) {
  Dispatcher dispatcher;
  VectorTask task(/*use_make_constructor=*/false);
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  pw::Vector<int, 2> other = {0, 1};
  task.sender()->Set(other);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.value()[0], 0);
  EXPECT_EQ(task.value()[1], 1);
}

TEST(OnceSender, OnceRefSenderModify) {
  Dispatcher dispatcher;
  VectorTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.sender()->ModifyUnsafe([](pw::Vector<int>& vec) { vec.push_back(0); });
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.sender()->ModifyUnsafe([](pw::Vector<int>& vec) { vec.push_back(1); });
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.sender()->Commit();
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.value()[0], 0);
  EXPECT_EQ(task.value()[1], 1);
}

TEST(OnceSender, DestroyingOnceRefSenderCausesReceiverPendToReturnCancelled) {
  Dispatcher dispatcher;
  VectorTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  task.DestroySender();
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  task.DestroyReceiver();

  ASSERT_TRUE(task.ready_value().has_value());
  ASSERT_TRUE(task.ready_value().value().IsCancelled());
}

class MoveOnlyRefTask : public Task {
 public:
  Poll<> DoPend(Context& cx) override {
    if (!receiver_) {
      auto [send, recv] =
          pw::async2::MakeOnceRefSenderAndReceiver<MoveOnlyValue>(value_);
      sender_.emplace(std::move(send));
      receiver_.emplace(std::move(recv));
    }
    Poll<pw::Status> poll = receiver_->Pend(cx);
    if (poll.IsReady()) {
      ready_value_.emplace(poll.value());
      return Ready();
    }
    return Pending();
  }

  MoveOnlyValue& value() { return value_; }
  std::optional<pw::Status>& ready_value() { return ready_value_; }
  std::optional<OnceRefSender<MoveOnlyValue>>& sender() { return sender_; }

 private:
  MoveOnlyValue value_{0};
  std::optional<pw::Status> ready_value_;
  std::optional<OnceRefReceiver<MoveOnlyValue>> receiver_;
  std::optional<OnceRefSender<MoveOnlyValue>> sender_;
};

TEST(OnceSender, OnceRefSenderSetRValue) {
  Dispatcher dispatcher;
  MoveOnlyRefTask task;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsPending());

  MoveOnlyValue value2(2);
  task.sender()->Set(std::move(value2));
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  ASSERT_TRUE(task.ready_value().has_value());
  ASSERT_TRUE(task.ready_value()->ok());
  EXPECT_EQ(task.value().value(), 2);
}

class AlreadyCompletedReceiverTask : public Task {
 public:
  AlreadyCompletedReceiverTask(OnceReceiver<MoveOnlyValue> receiver)
      : receiver_(std::move(receiver)) {}

  Poll<> DoPend(Context& cx) override {
    Poll<pw::Result<MoveOnlyValue>> poll = receiver_.Pend(cx);
    if (poll.IsReady()) {
      ready_value_.emplace(std::move(poll.value()));
      return Ready();
    }
    return Pending();
  }

  std::optional<pw::Result<MoveOnlyValue>>& ready_value() {
    return ready_value_;
  }

 private:
  std::optional<pw::Result<MoveOnlyValue>> ready_value_;
  OnceReceiver<MoveOnlyValue> receiver_;
};

TEST(OnceSender, OnceReceiverAlreadyCompleted) {
  Dispatcher dispatcher;
  OnceReceiver<MoveOnlyValue> receiver(2);
  AlreadyCompletedReceiverTask task(std::move(receiver));
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  ASSERT_TRUE(task.ready_value().has_value());
  ASSERT_TRUE(task.ready_value()->ok());
  EXPECT_EQ(task.ready_value()->value().value(), 2);
}

}  // namespace
