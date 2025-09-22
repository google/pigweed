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
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::experimental::ListableFutureWithWaker;
using pw::async2::experimental::ListFutureProvider;

class SimpleIntFuture;

class SimpleAsyncInt {
 public:
  SimpleIntFuture Get();

  void Set(int value) {
    {
      std::lock_guard lock(provider_.lock());
      PW_ASSERT(!value_.has_value());
      value_ = value;
    }
    ResolveAllFutures();
  }

 private:
  friend class SimpleIntFuture;

  void ResolveAllFutures();

  ListFutureProvider<SimpleIntFuture> provider_;
  std::optional<int> value_;
};

class SimpleIntFuture : public ListableFutureWithWaker<SimpleIntFuture, int> {
 public:
  SimpleIntFuture(SimpleIntFuture&& other) noexcept
      : ListableFutureWithWaker(kMovedFrom),
        provider_(std::exchange(other.provider_, nullptr)) {
    ListableFutureWithWaker::MoveFrom(other);
  }

  SimpleIntFuture& operator=(SimpleIntFuture&& other) noexcept {
    provider_ = std::exchange(other.provider_, nullptr);
    ListableFutureWithWaker::MoveFrom(other);
    return *this;
  }

  pw::async2::Poll<int> DoPend(pw::async2::Context&) {
    PW_ASSERT(provider_ != nullptr);
    std::lock_guard guard(lock());

    if (!provider_->value_.has_value()) {
      return pw::async2::Pending();
    }

    return provider_->value_.value();
  }

 private:
  friend class SimpleAsyncInt;
  friend class ListableFutureWithWaker<SimpleIntFuture, int>;

  SimpleIntFuture(SimpleAsyncInt& provider)
      : ListableFutureWithWaker(provider.provider_), provider_(&provider) {}

  SimpleAsyncInt* provider_;
};

SimpleIntFuture SimpleAsyncInt::Get() { return SimpleIntFuture(*this); }

void SimpleAsyncInt::ResolveAllFutures() {
  while (!provider_.empty()) {
    SimpleIntFuture& future = provider_.Pop();
    future.Wake();
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

TEST(FutureProvider, MultipleFutures) {
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

}  // namespace
