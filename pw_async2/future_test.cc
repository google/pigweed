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

#include "pw_async2/future.h"

#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/try.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::experimental::ListableFutureWithWaker;
using pw::async2::experimental::ListFutureProvider;
using pw::async2::experimental::SingleFutureProvider;

class SimpleIntFuture;

class SimpleAsyncInt {
 public:
  SimpleIntFuture Get();
  std::optional<SimpleIntFuture> GetSingle();

  void Set(int value) {
    {
      std::lock_guard lock(list_provider_.lock());
      PW_ASSERT(!value_.has_value());
      value_ = value;
    }
    ResolveAllFutures();
  }

 private:
  friend class SimpleIntFuture;

  void ResolveAllFutures();

  // This object stores both a list provider and a single provider for testing
  // purposes. In actual usage, only one of these would be needed, depending on
  // how many consumers the operation allows.
  ListFutureProvider<SimpleIntFuture> list_provider_;
  SingleFutureProvider<SimpleIntFuture> single_provider_;
  std::optional<int> value_;
};

class SimpleIntFuture : public ListableFutureWithWaker<SimpleIntFuture, int> {
 public:
  static constexpr const char kWaitReason[] = "SimpleIntFuture";

  SimpleIntFuture(SimpleIntFuture&& other) noexcept
      : ListableFutureWithWaker(kMovedFrom),
        async_int_(std::exchange(other.async_int_, nullptr)) {
    ListableFutureWithWaker::MoveFrom(other);
  }

  SimpleIntFuture& operator=(SimpleIntFuture&& other) noexcept {
    async_int_ = std::exchange(other.async_int_, nullptr);
    ListableFutureWithWaker::MoveFrom(other);
    return *this;
  }

  pw::async2::Poll<int> DoPend(pw::async2::Context&) {
    PW_ASSERT(async_int_ != nullptr);
    std::lock_guard guard(lock());

    if (!async_int_->value_.has_value()) {
      return pw::async2::Pending();
    }

    return async_int_->value_.value();
  }

 private:
  friend class SimpleAsyncInt;
  friend class ListableFutureWithWaker<SimpleIntFuture, int>;

  SimpleIntFuture(SimpleAsyncInt& async_int,
                  ListFutureProvider<SimpleIntFuture>& provider)
      : ListableFutureWithWaker(provider), async_int_(&async_int) {}

  SimpleIntFuture(SimpleAsyncInt& async_int,
                  SingleFutureProvider<SimpleIntFuture>& provider)
      : ListableFutureWithWaker(provider), async_int_(&async_int) {}

  SimpleAsyncInt* async_int_;
};

SimpleIntFuture SimpleAsyncInt::Get() {
  return SimpleIntFuture(*this, list_provider_);
}

std::optional<SimpleIntFuture> SimpleAsyncInt::GetSingle() {
  if (!single_provider_.has_future()) {
    return SimpleIntFuture(*this, single_provider_);
  }
  return std::nullopt;
}

void SimpleAsyncInt::ResolveAllFutures() {
  while (!list_provider_.empty()) {
    SimpleIntFuture& future = list_provider_.Pop();
    future.Wake();
  }

  if (single_provider_.has_future()) {
    single_provider_.Take().Wake();
  }
}

static_assert(!pw::async2::experimental::is_future_v<int>);
static_assert(pw::async2::experimental::is_future_v<SimpleIntFuture>);

TEST(Future, Pend) {
  pw::async2::Dispatcher dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future = provider.Get();
  int result = -1;

  pw::async2::PendFuncTask task(
      [&](pw::async2::Context& cx) -> pw::async2::Poll<> {
        PW_TRY_READY_ASSIGN(int value, future.Pend(cx));
        result = value;
        return pw::async2::Ready();
      });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Pending());

  provider.Set(27);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Ready());
  EXPECT_EQ(result, 27);
}

TEST(Future, MoveAssign) {
  pw::async2::Dispatcher dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future1 = provider.Get();

  SimpleIntFuture future2 = provider.Get();
  future1 = std::move(future2);

  int result = -1;
  pw::async2::PendFuncTask task(
      [&](pw::async2::Context& cx) -> pw::async2::Poll<> {
        PW_TRY_READY_ASSIGN(int value, future1.Pend(cx));
        result = value;
        return pw::async2::Ready();
      });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Pending());

  provider.Set(99);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Ready());
  EXPECT_EQ(result, 99);
}

TEST(Future, MoveConstruct) {
  pw::async2::Dispatcher dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future1 = provider.Get();
  SimpleIntFuture future2(std::move(future1));

  int result = -1;
  pw::async2::PendFuncTask task(
      [&](pw::async2::Context& cx) -> pw::async2::Poll<> {
        PW_TRY_READY_ASSIGN(int value, future2.Pend(cx));
        result = value;
        return pw::async2::Ready();
      });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Pending());

  provider.Set(99);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Ready());
  EXPECT_EQ(result, 99);
}

TEST(Future, DestroyBeforeCompletion) {
  pw::async2::Dispatcher dispatcher;
  SimpleAsyncInt provider;

  {
    [[maybe_unused]] SimpleIntFuture future = provider.Get();
  }

  // The provider should not crash by waking a nonexistent future.
  provider.Set(99);
}

TEST(ListFutureProvider, MultipleFutures) {
  pw::async2::Dispatcher dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future1 = provider.Get();
  SimpleIntFuture future2 = provider.Get();
  int result1 = -1;
  int result2 = -1;

  pw::async2::PendFuncTask task1(
      [&](pw::async2::Context& cx) -> pw::async2::Poll<> {
        PW_TRY_READY_ASSIGN(int value, future1.Pend(cx));
        result1 = value;
        return pw::async2::Ready();
      });

  pw::async2::PendFuncTask task2(
      [&](pw::async2::Context& cx) -> pw::async2::Poll<> {
        PW_TRY_READY_ASSIGN(int value, future2.Pend(cx));
        result2 = value;
        return pw::async2::Ready();
      });

  dispatcher.Post(task1);
  dispatcher.Post(task2);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Pending());

  provider.Set(33);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Ready());
  EXPECT_EQ(result1, 33);
  EXPECT_EQ(result2, 33);
}

TEST(SingleFutureProvider, VendsAndResolvesFuture) {
  pw::async2::Dispatcher dispatcher;
  SimpleAsyncInt provider;

  std::optional<SimpleIntFuture> future = provider.GetSingle();

  ASSERT_TRUE(future.has_value());

  int result = -1;
  pw::async2::PendFuncTask task(
      [&](pw::async2::Context& cx) -> pw::async2::Poll<> {
        PW_TRY_READY_ASSIGN(int value, future->Pend(cx));
        result = value;
        return pw::async2::Ready();
      });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Pending());

  provider.Set(96);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Ready());
  EXPECT_EQ(result, 96);
}

TEST(SingleFutureProvider, OnlyAllowsOneFutureToExist) {
  pw::async2::Dispatcher dispatcher;
  SimpleAsyncInt provider;

  {
    std::optional<SimpleIntFuture> future1 = provider.GetSingle();
    std::optional<SimpleIntFuture> future2 = provider.GetSingle();
    EXPECT_TRUE(future1.has_value());
    EXPECT_FALSE(future2.has_value());
  }

  // `future1` went out of scope, so we should be allowed to get a new one.
  std::optional<SimpleIntFuture> future = provider.GetSingle();
  ASSERT_TRUE(future.has_value());

  int result = -1;
  pw::async2::PendFuncTask task(
      [&](pw::async2::Context& cx) -> pw::async2::Poll<> {
        PW_TRY_READY_ASSIGN(int value, future->Pend(cx));
        result = value;
        return pw::async2::Ready();
      });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Pending());

  provider.Set(93);
  EXPECT_EQ(dispatcher.RunUntilStalled(), pw::async2::Ready());
  EXPECT_EQ(result, 93);

  // The operation has resolved, so a new future should be obtainable.
  std::optional<SimpleIntFuture> new_future = provider.GetSingle();
  ASSERT_TRUE(new_future.has_value());
}

#if PW_NC_TEST(FutureWaitReasonMustBeCharArray)
PW_NC_EXPECT("kWaitReason must be a character array");
class BadFuture : public ListableFutureWithWaker<BadFuture, int> {
 public:
  BadFuture() : ListableFutureWithWaker(kMovedFrom) {}
  static constexpr const char* kWaitReason = "this is a char* not an array";
  pw::async2::Poll<int> DoPend(pw::async2::Context&) { return 5; }
};

void ShouldAssert() {
  BadFuture future;
  pw::async2::PendFuncTask task(
      [&](pw::async2::Context& cx) { return future.Pend(cx).Readiness(); });
}
#endif  // PW_NC_TEST

}  // namespace
