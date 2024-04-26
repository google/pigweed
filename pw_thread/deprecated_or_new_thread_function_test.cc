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

#include "pw_thread/deprecated_or_new_thread_function.h"

#include "pw_unit_test/framework.h"

namespace {

using ::pw::Function;
using ::pw::thread::DeprecatedFnPtrAndArg;
using ::pw::thread::DeprecatedOrNewThreadFn;

TEST(DeprecatedOrNewThreadFunction, CallInvokesLambda) {
  int call_count = 0;
  DeprecatedOrNewThreadFn fn;
  fn = Function<void()>([&call_count]() { ++call_count; });
  EXPECT_EQ(call_count, 0);
  fn();
  EXPECT_EQ(call_count, 1);
}

TEST(DeprecatedOrNewThreadFunction, CallMovedInvokesLambda) {
  int call_count = 0;
  DeprecatedOrNewThreadFn fn;
  fn = Function<void()>([&call_count]() { ++call_count; });
  DeprecatedOrNewThreadFn fn2;
  fn2 = std::move(fn);
  EXPECT_EQ(call_count, 0);
  fn2();
  EXPECT_EQ(call_count, 1);
}

class DestroyCounter {
 public:
  DestroyCounter(int& counter) : counter_(&counter) {}
  DestroyCounter(const DestroyCounter&) = delete;
  DestroyCounter& operator=(const DestroyCounter&) = delete;
  DestroyCounter(DestroyCounter&& other) : counter_(other.counter_) {
    other.counter_ = nullptr;
  }
  DestroyCounter& operator=(DestroyCounter&& other) {
    CounterIncrement();
    counter_ = other.counter_;
    other.counter_ = nullptr;
    return *this;
  }
  ~DestroyCounter() { CounterIncrement(); }

 private:
  void CounterIncrement() {
    if (counter_ != nullptr) {
      ++(*counter_);
    }
  }
  int* counter_;
};

TEST(DeprecatedOrNewThreadFunction, DestructionInvokesLambdasDestructor) {
  int counter = 0;
  {
    DeprecatedOrNewThreadFn fn;
    fn = Function<void()>([_unused = DestroyCounter(counter)]() {});
    EXPECT_EQ(counter, 0);
  }
  EXPECT_EQ(counter, 1);
}

TEST(DeprecatedOrNewThreadFunction, DestructionMovedInvokesLambdasDestructor) {
  int counter = 0;
  {
    DeprecatedOrNewThreadFn fn;
    {
      fn = Function<void()>([_unused = DestroyCounter(counter)]() {});
      DeprecatedOrNewThreadFn scoped_fn;
      scoped_fn = std::move(fn);
      EXPECT_EQ(counter, 0);
    }
    EXPECT_EQ(counter, 1);
  }
  EXPECT_EQ(counter, 1);
}

TEST(DeprecatedOrNewThreadFunction, NullptrAssignmentInvokesLambdasDestructor) {
  int counter = 0;
  {
    DeprecatedOrNewThreadFn fn;
    fn = Function<void()>([_unused = DestroyCounter(counter)]() {});
    EXPECT_EQ(counter, 0);
    fn = nullptr;
    EXPECT_EQ(counter, 1);
  }
  EXPECT_EQ(counter, 1);
}

void increment(void* int_to_increment) {
  int& value = *static_cast<int*>(int_to_increment);
  ++value;
}

TEST(DeprecatedOrNewThreadFunction, CallInvokesFnPtrWithArg) {
  int call_count = 0;
  DeprecatedOrNewThreadFn fn;
  fn = DeprecatedFnPtrAndArg{increment, static_cast<void*>(&call_count)};
  EXPECT_EQ(call_count, 0);
  fn();
  EXPECT_EQ(call_count, 1);
}

TEST(DeprecatedOrNewThreadFunction, CallMovedInvokesFnPtrWithArg) {
  int call_count = 0;
  DeprecatedOrNewThreadFn fn;
  fn = DeprecatedFnPtrAndArg{increment, static_cast<void*>(&call_count)};
  DeprecatedOrNewThreadFn fn2;
  fn2 = std::move(fn);
  EXPECT_EQ(call_count, 0);
  fn2();
  EXPECT_EQ(call_count, 1);
}

}  // namespace
