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

#include "pw_async2/join.h"

#include "pw_async2/dispatcher.h"
#include "pw_async2/value_future.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::Join;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Waker;
using ::pw::async2::experimental::BroadcastValueProvider;

// Windows GCC emits a bogs uninitialized error for the
// move constructor below.
PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC_GCC(ignored, "-Wmaybe-uninitialized");
struct SomeMoveOnlyValue {
  explicit SomeMoveOnlyValue(int result) : result_(result), move_count_(0) {}
  SomeMoveOnlyValue(const SomeMoveOnlyValue&) = delete;
  SomeMoveOnlyValue& operator=(const SomeMoveOnlyValue&) = delete;
  SomeMoveOnlyValue(SomeMoveOnlyValue&& other)
      : result_(other.result_), move_count_(other.move_count_ + 1) {
    other.result_ = -1;
  }
  SomeMoveOnlyValue& operator=(SomeMoveOnlyValue&& other) {
    result_ = other.result_;
    other.result_ = -1;
    move_count_ = other.move_count_ + 1;
    return *this;
  }
  ~SomeMoveOnlyValue() = default;
  int result_;
  int move_count_;
};
PW_MODIFY_DIAGNOSTICS_POP();

struct PendableController {
  PendableController() : poll_count_(0), allow_completion_(false), waker_() {}
  PendableController(PendableController&) = delete;
  PendableController(PendableController&&) = delete;
  PendableController& operator=(const PendableController&) = delete;
  PendableController& operator=(PendableController&&) = delete;
  ~PendableController() = default;

  int poll_count_;
  bool allow_completion_;
  Waker waker_;
};

struct StructWithPendMethod {
  StructWithPendMethod(int result, PendableController& controller)
      : result_(result), controller_(&controller) {}

  StructWithPendMethod(const StructWithPendMethod&) = delete;
  StructWithPendMethod& operator=(const StructWithPendMethod&) = delete;
  StructWithPendMethod(StructWithPendMethod&&) = default;
  StructWithPendMethod& operator=(StructWithPendMethod&&) = default;
  ~StructWithPendMethod() = default;

  Poll<SomeMoveOnlyValue> Pend(Context& cx) {
    ++controller_->poll_count_;
    if (controller_->allow_completion_) {
      return Poll<SomeMoveOnlyValue>(result_);
    }
    PW_ASYNC_STORE_WAKER(
        cx,
        controller_->waker_,
        "StructWithPendMethod is waiting for PendableController's waker");
    return Pending();
  }

  int result_;
  PendableController* controller_;
};

TEST(Join, PendDelegatesToPendables) {
  Dispatcher dispatcher;

  PendableController controller_1;
  PendableController controller_2;
  StructWithPendMethod pendable_1(1, controller_1);
  StructWithPendMethod pendable_2(2, controller_2);
  Join join(std::move(pendable_1), std::move(pendable_2));

  EXPECT_EQ(dispatcher.RunPendableUntilStalled(join), Pending());
  controller_2.allow_completion_ = true;
  std::move(controller_2.waker_).Wake();
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(join), Pending());
  controller_1.allow_completion_ = true;
  std::move(controller_1.waker_).Wake();
  auto&& result = dispatcher.RunPendableUntilStalled(join);
  ASSERT_TRUE(result.IsReady());
  auto&& [v1, v2] = std::move(*result);
  EXPECT_EQ(v1.result_, 1);
  EXPECT_EQ(v2.result_, 2);
  EXPECT_EQ(v1.move_count_, 1);
  EXPECT_EQ(v2.move_count_, 1);
}

TEST(Join, BindsDirectly) {
  Dispatcher dispatcher;
  PendableController controller_1;
  controller_1.allow_completion_ = true;
  PendableController controller_2;
  controller_2.allow_completion_ = true;
  StructWithPendMethod pendable_1(1, controller_1);
  StructWithPendMethod pendable_2(2, controller_2);
  Join join(std::move(pendable_1), std::move(pendable_2));

  auto&& [v1, v2] = dispatcher.RunPendableToCompletion(join);
  EXPECT_EQ(v1.result_, 1);
  EXPECT_EQ(v2.result_, 2);
  EXPECT_EQ(v1.move_count_, 1);
  EXPECT_EQ(v2.move_count_, 1);
}

TEST(JoinFuture, ReturnsReadyWhenAllPendablesAreReady) {
  Dispatcher dispatcher;

  BroadcastValueProvider<int> int_provider;
  BroadcastValueProvider<char> char_provider;

  auto future =
      ::pw::async2::experimental::Join(int_provider.Get(), char_provider.Get());
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(future), Pending());
  int_provider.Resolve(43);
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(future), Pending());
  char_provider.Resolve('d');
  auto&& result = dispatcher.RunPendableUntilStalled(future);
  ASSERT_TRUE(result.IsReady());
  auto&& [i, c] = *result;
  EXPECT_EQ(i, 43);
  EXPECT_EQ(c, 'd');
  EXPECT_TRUE(future.is_complete());
}

#if PW_NC_TEST(ArgumentsToJoinMustBeFutures)
PW_NC_EXPECT("All arguments to Join must be Future types");
void ShouldAssert() {
  auto not_a_future = []() -> int { return 42; };
  auto future = ::pw::async2::experimental::Join(not_a_future());
}
#endif  // PW_NC_TEST

}  // namespace
