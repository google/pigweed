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

#include "pw_async2/pendable.h"

#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"
#include "pw_async2/waker.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::PendableFor;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::ReadyType;
using ::pw::async2::Waker;
using ::pw::async2::internal::IsPendable;
using ::pw::async2::internal::PendableTraits;

struct PendableValue {
  PendableValue(int value)
      : poll_count(0), value_(value), allow_completion_(false), waker_() {}

  Poll<int> Get(Context& cx) {
    ++poll_count;
    if (allow_completion_) {
      return Ready(value_);
    }
    PW_ASYNC_STORE_WAKER(cx, waker_, "PendableValue waiting for value");
    return Pending();
  }

  Poll<int> GetAndOffset(Context& cx, unsigned amt, bool subtract) {
    if (Poll<int> result = Get(cx); result.IsReady()) {
      if (subtract) {
        return Ready(*result - static_cast<int>(amt));
      }
      return Ready(*result + static_cast<int>(amt));
    }
    return Pending();
  }

  void AllowCompletion() {
    allow_completion_ = true;
    std::move(waker_).Wake();
  }

  int poll_count;

 private:
  int value_;
  bool allow_completion_;
  Waker waker_;
};

Waker always_pending_waker;
Poll<> AlwaysPending(Context& cx) {
  PW_ASYNC_STORE_WAKER(cx, always_pending_waker, "AlwaysPending waker");
  return Pending();
}
Poll<bool> AlwaysReady(Context&) { return Ready(true); }
Poll<char> AlwaysReadyWithValue(Context&, char value) { return Ready(value); }

struct ByReference {
  int value = 0;
};

Poll<> IncrementReference(Context&, ByReference& ref, int amt) {
  ref.value += amt;
  return Ready();
}

int Add(int a, int b) { return a + b; }

//////// IsPendable

static_assert(IsPendable<&PendableValue::Get>);
static_assert(IsPendable<&PendableValue::GetAndOffset>);
static_assert(IsPendable<&AlwaysPending>);
static_assert(IsPendable<&AlwaysReady>);
static_assert(IsPendable<&AlwaysReadyWithValue>);
static_assert(IsPendable<&IncrementReference>);

static_assert(!IsPendable<&PendableValue::AllowCompletion>);
static_assert(!IsPendable<&PendableValue::poll_count>);
static_assert(!IsPendable<&Add>);

//////// PendableTraits::Output

static_assert(
    std::is_same_v<PendableTraits<decltype(&PendableValue::Get)>::Output, int>);
static_assert(std::is_same_v<PendableTraits<decltype(&AlwaysPending)>::Output,
                             ReadyType>);
static_assert(
    std::is_same_v<PendableTraits<decltype(&AlwaysReady)>::Output, bool>);

//////// PendableTraits::Class

static_assert(
    std::is_same_v<PendableTraits<decltype(&PendableValue::Get)>::Class,
                   PendableValue>);
static_assert(std::is_same_v<
              PendableTraits<decltype(&PendableValue::GetAndOffset)>::Class,
              PendableValue>);

//////// PendableTraits::Arguments

static_assert(
    std::is_same_v<PendableTraits<decltype(&PendableValue::Get)>::Arguments,
                   std::tuple<>>);
static_assert(std::is_same_v<
              PendableTraits<decltype(&PendableValue::GetAndOffset)>::Arguments,
              std::tuple<unsigned, bool>>);
static_assert(
    std::is_same_v<PendableTraits<decltype(&AlwaysPending)>::Arguments,
                   std::tuple<>>);
static_assert(
    std::is_same_v<PendableTraits<decltype(&AlwaysReadyWithValue)>::Arguments,
                   std::tuple<char>>);
static_assert(
    std::is_same_v<PendableTraits<decltype(&IncrementReference)>::Arguments,
                   std::tuple<ByReference&, int>>);

TEST(MemberPendableWrapper, InvokesFunctionWithoutArgs) {
  Dispatcher dispatcher;
  PendableValue value(5);
  auto wrapper = PendableFor<&PendableValue::Get>(value);
  EXPECT_FALSE(wrapper.completed());
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper), Pending());
  EXPECT_EQ(value.poll_count, 1);
  EXPECT_FALSE(wrapper.completed());
  value.AllowCompletion();
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper), Ready(5));
  EXPECT_EQ(value.poll_count, 2);
  EXPECT_TRUE(wrapper.completed());
}

TEST(MemberPendableWrapper, InvokesFunctionWithArgs) {
  Dispatcher dispatcher;
  PendableValue value(5);
  auto wrapper = PendableFor<&PendableValue::GetAndOffset>(value, 7, true);
  EXPECT_FALSE(wrapper.completed());
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper), Pending());
  EXPECT_EQ(value.poll_count, 1);
  EXPECT_FALSE(wrapper.completed());
  value.AllowCompletion();
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper), Ready(-2));
  EXPECT_EQ(value.poll_count, 2);
  EXPECT_TRUE(wrapper.completed());
}

TEST(MemberPendableWrapper, MoveConstruct) {
  Dispatcher dispatcher;
  PendableValue value(5);
  auto wrapper1 = PendableFor<&PendableValue::Get>(value);
  auto wrapper2(std::move(wrapper1));
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper2), Pending());
  EXPECT_EQ(value.poll_count, 1);
}

TEST(MemberPendableWrapper, MoveAssign) {
  Dispatcher dispatcher;
  PendableValue five(5);
  PendableValue six(6);
  auto wrapper1 = PendableFor<&PendableValue::Get>(five);
  auto wrapper2 = PendableFor<&PendableValue::Get>(six);
  wrapper2 = std::move(wrapper1);
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper2), Pending());
  EXPECT_EQ(five.poll_count, 1);
  EXPECT_EQ(six.poll_count, 0);
}

TEST(FreePendableWrapper, InvokesFunctionWithoutArgs) {
  Dispatcher dispatcher;
  auto pending_wrapper = PendableFor<&AlwaysPending>();
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(pending_wrapper), Pending());
  auto ready_wrapper = PendableFor<&AlwaysReady>();
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(ready_wrapper), Ready(true));
}

TEST(FreePendableWrapper, InvokesFunctionWithArgs) {
  Dispatcher dispatcher;
  auto ready_wrapper = PendableFor<&AlwaysReadyWithValue>('h');
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(ready_wrapper), Ready('h'));
}

TEST(FreePendableWrapper, MoveConstruct) {
  Dispatcher dispatcher;
  auto wrapper1 = PendableFor<&AlwaysPending>();
  auto wrapper2(std::move(wrapper1));
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper2), Pending());
}

TEST(FreePendableWrapper, MoveAssign) {
  Dispatcher dispatcher;
  auto wrapper1 = PendableFor<&AlwaysReadyWithValue>('x');
  auto wrapper2 = PendableFor<&AlwaysReadyWithValue>('y');
  wrapper2 = std::move(wrapper1);
  EXPECT_EQ(dispatcher.RunPendableUntilStalled(wrapper2), Ready('x'));
}

TEST(FreePendableWrapper, Reference) {
  Dispatcher dispatcher;
  ByReference ref{.value = 3};
  auto wrapper = PendableFor<&IncrementReference>(ref, 7);
  EXPECT_TRUE(dispatcher.RunPendableUntilStalled(wrapper).IsReady());
  EXPECT_EQ(ref.value, 10);
}

}  // namespace
