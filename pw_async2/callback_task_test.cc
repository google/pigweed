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

#include "pw_async2/callback_task.h"

#include <optional>

#include "pw_async2/dispatcher.h"
#include "pw_async2/waker.h"
#include "pw_function/function.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::Function;
using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::OneshotCallbackTask;
using ::pw::async2::OneshotCallbackTaskFor;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::RecurringCallbackTask;
using ::pw::async2::RecurringCallbackTaskFor;
using ::pw::async2::Waker;

std::optional<char> fake_uart_char;
Waker fake_uart_waker;

void InitializeUart() {
  fake_uart_char = std::nullopt;
  fake_uart_waker.Clear();
}

void SetUartData(char c) {
  fake_uart_char = c;
  std::move(fake_uart_waker).Wake();
}

Poll<char> FakeReadUart(Context& cx) {
  if (fake_uart_char.has_value()) {
    char c = *fake_uart_char;
    fake_uart_char = std::nullopt;
    return Ready(c);
  }

  PW_ASYNC_STORE_WAKER(cx, fake_uart_waker, "FakeReadUart waiting for data");
  return Pending();
}

int ready_immediately_max_ready_before_unschedule = 1;
Poll<int> ReadyImmediately(Context& cx) {
  if (ready_immediately_max_ready_before_unschedule-- > 0) {
    return Ready(7);
  }
  return cx.Unschedule<int>();
}

Poll<> ReturnNothingImmediately(Context&) { return Ready(); }

template <typename T>
class CallbackContext {
 public:
  CallbackContext(T initial_value) : value_(initial_value) {}

  void set_value(T value) {
    invocation_count_++;
    value_ = value;
  }

  constexpr T value() const { return value_; }
  constexpr int invocation_count() const { return invocation_count_; }

 private:
  T value_;
  int invocation_count_ = 0;
};

class PendableReader {
 public:
  Poll<int> ReadValue(Context& cx) {
    if (value_.has_value()) {
      int value = *value_;
      value_ = std::nullopt;
      return Ready(value);
    }

    PW_ASYNC_STORE_WAKER(cx, waker_, "PendableReader waiting for value");
    return Pending();
  }

  void SetValue(int value) {
    value_ = value;
    std::move(waker_).Wake();
  }

 private:
  std::optional<int> value_;
  Waker waker_;
};

TEST(OneshotCallbackTask, FreeFunctionWithArguments) {
  InitializeUart();

  CallbackContext<char> callback_context('\0');

  OneshotCallbackTask<char> task = OneshotCallbackTaskFor<&FakeReadUart>(
      [&callback_context](char c) { callback_context.set_value(c); });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), '\0');
  EXPECT_EQ(callback_context.invocation_count(), 0);

  SetUartData('b');
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 'b');
  EXPECT_EQ(callback_context.invocation_count(), 1);

  // A oneshot task should not run a second time.
  SetUartData('d');
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 'b');
  EXPECT_EQ(callback_context.invocation_count(), 1);
}

TEST(OneshotCallbackTask, FreeFunctionWithoutArguments) {
  InitializeUart();

  CallbackContext<int> callback_context(0);

  OneshotCallbackTask<> task =
      OneshotCallbackTaskFor<&ReturnNothingImmediately>(
          [&callback_context]() { callback_context.set_value(1); });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 1);
  EXPECT_EQ(callback_context.invocation_count(), 1);

  // A oneshot task should not run a second time.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 1);
  EXPECT_EQ(callback_context.invocation_count(), 1);
}

TEST(OneshotCallbackTask, MemberFunction) {
  CallbackContext<int> callback_context(0);

  PendableReader pendable_reader;
  OneshotCallbackTask<int> task =
      OneshotCallbackTaskFor<&PendableReader::ReadValue>(
          pendable_reader, [&callback_context](int value) {
            callback_context.set_value(value);
          });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 0);
  EXPECT_EQ(callback_context.invocation_count(), 0);

  pendable_reader.SetValue(27);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 27);
  EXPECT_EQ(callback_context.invocation_count(), 1);

  // A oneshot task should not run a second time.
  pendable_reader.SetValue(39);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 27);
  EXPECT_EQ(callback_context.invocation_count(), 1);
}

TEST(OneshotCallbackTask, ImmediatelyReturnsReady) {
  CallbackContext<int> callback_context(0);

  ready_immediately_max_ready_before_unschedule = 1;
  OneshotCallbackTask<int> task = OneshotCallbackTaskFor<&ReadyImmediately>(
      [&callback_context](int value) { callback_context.set_value(value); });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 7);
  EXPECT_EQ(callback_context.invocation_count(), 1);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 7);
  EXPECT_EQ(callback_context.invocation_count(), 1);
}

TEST(RecurringCallbackTask, FreeFunction) {
  InitializeUart();

  CallbackContext<char> callback_context('\0');

  RecurringCallbackTask<char> task = RecurringCallbackTaskFor<&FakeReadUart>(
      [&callback_context](char c) { callback_context.set_value(c); });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), '\0');
  EXPECT_EQ(callback_context.invocation_count(), 0);

  // A recurring task should re-run forever without completing.
  SetUartData('b');
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 'b');
  EXPECT_EQ(callback_context.invocation_count(), 1);

  SetUartData('d');
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 'd');
  EXPECT_EQ(callback_context.invocation_count(), 2);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 'd');
  EXPECT_EQ(callback_context.invocation_count(), 2);

  SetUartData('g');
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 'g');
  EXPECT_EQ(callback_context.invocation_count(), 3);
}

TEST(RecurringCallbackTask, MemberFunction) {
  CallbackContext<int> callback_context(0);

  PendableReader pendable_reader;
  RecurringCallbackTask<int> task =
      RecurringCallbackTaskFor<&PendableReader::ReadValue>(
          pendable_reader, [&callback_context](int value) {
            callback_context.set_value(value);
          });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 0);
  EXPECT_EQ(callback_context.invocation_count(), 0);

  // A recurring task should re-run forever without completing.
  pendable_reader.SetValue(27);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 27);
  EXPECT_EQ(callback_context.invocation_count(), 1);

  pendable_reader.SetValue(39);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 39);
  EXPECT_EQ(callback_context.invocation_count(), 2);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 39);
  EXPECT_EQ(callback_context.invocation_count(), 2);

  pendable_reader.SetValue(51);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(callback_context.value(), 51);
  EXPECT_EQ(callback_context.invocation_count(), 3);
}

TEST(RecurringCallbackTask, ImmediatelyReturnsReady) {
  CallbackContext<int> callback_context(0);

  constexpr int kMaxInvocations = 10;

  ready_immediately_max_ready_before_unschedule = kMaxInvocations;
  RecurringCallbackTask<int> task = RecurringCallbackTaskFor<&ReadyImmediately>(
      [&callback_context](int value) { callback_context.set_value(value); });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(callback_context.value(), 7);
  EXPECT_EQ(callback_context.invocation_count(), kMaxInvocations);
}

}  // namespace
