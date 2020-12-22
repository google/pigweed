// Copyright 2020 The Pigweed Authors
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

#include <atomic>

#include "gtest/gtest.h"
#include "pw_thread/id.h"
#include "pw_thread/test_threads.h"
#include "pw_thread/thread.h"

using pw::thread::test::TestOptionsThread0;
using pw::thread::test::TestOptionsThread1;

namespace pw::thread {
namespace {

TEST(Thread, DefaultIds) {
  Thread not_executing_thread;
  EXPECT_EQ(not_executing_thread.get_id(), Id());
}

static void SetBoolTrue(void* arg) {
  *reinterpret_cast<std::atomic<bool>*>(arg) = true;
}

// TODO(ewout): this currently doesn't work on backends with dynamic context
// allocation disabled. Perhaps we could require the backend to provide
// thread options for the facade unit tests to use?

#if PW_THREAD_JOINING_ENABLED
TEST(Thread, Join) {
  Thread thread;
  EXPECT_FALSE(thread.joinable());
  std::atomic<bool> thread_ran = false;
  thread = Thread(TestOptionsThread0(), SetBoolTrue, &thread_ran);
  EXPECT_TRUE(thread.joinable());
  thread.join();
  EXPECT_EQ(thread.get_id(), Id());
  EXPECT_TRUE(thread_ran);
}
#endif  // PW_THREAD_JOINING_ENABLED

TEST(Thread, Detach) {
  Thread thread;
  std::atomic<bool> thread_ran = false;
  thread = Thread(TestOptionsThread0(), SetBoolTrue, &thread_ran);
  EXPECT_NE(thread.get_id(), Id());
#if PW_THREAD_JOINING_ENABLED
  EXPECT_TRUE(thread.joinable());
#endif  // PW_THREAD_JOINING_ENABLED
  thread.detach();
  EXPECT_EQ(thread.get_id(), Id());
#if PW_THREAD_JOINING_ENABLED
  EXPECT_FALSE(thread.joinable());
#endif  // PW_THREAD_JOINING_ENABLED
  // We could use a synchronization primitive here to wait until the thread
  // finishes running to check that thread_ran is true, but that's covered by
  // pw_sync instead. Instead we use an idiotic busy loop.
  // - Assume our clock is < 6Ghz
  // - Assume we can check the clock in a single cycle
  // - Wait for up to 1/10th of a second @ 6Ghz, this may be a long period on a
  //   slower (i.e. real) machine.
  constexpr uint64_t kMaxIterations = 6'000'000'000 / 10;
  for (uint64_t i = 0; i < kMaxIterations; ++i) {
    if (thread_ran)
      break;
  }
  EXPECT_TRUE(thread_ran);
}

TEST(Thread, SwapWithoutExecution) {
  Thread thread_0;
  Thread thread_1;

  // Make sure we can swap threads which are not associated with any execution.
  thread_0.swap(thread_1);
}

TEST(Thread, SwapWithOneExecuting) {
  Thread thread_0;
  EXPECT_EQ(thread_0.get_id(), Id());

  static std::atomic<bool> thread_ran = false;
  Thread thread_1(TestOptionsThread1(), SetBoolTrue, &thread_ran);
  EXPECT_NE(thread_1.get_id(), Id());

  thread_0.swap(thread_1);
  EXPECT_NE(thread_0.get_id(), Id());
  EXPECT_EQ(thread_1.get_id(), Id());

  thread_0.detach();
  EXPECT_EQ(thread_0.get_id(), Id());
}

TEST(Thread, SwapWithTwoExecuting) {
  static std::atomic<bool> thread_a_ran = false;
  Thread thread_0(TestOptionsThread0(), SetBoolTrue, &thread_a_ran);
  static std::atomic<bool> thread_b_ran = false;
  Thread thread_1(TestOptionsThread1(), SetBoolTrue, &thread_b_ran);
  const Id thread_a_id = thread_0.get_id();
  EXPECT_NE(thread_a_id, Id());
  const Id thread_b_id = thread_1.get_id();
  EXPECT_NE(thread_b_id, Id());
  EXPECT_NE(thread_a_id, thread_b_id);

  thread_0.swap(thread_1);
  EXPECT_EQ(thread_1.get_id(), thread_a_id);
  EXPECT_EQ(thread_0.get_id(), thread_b_id);

  thread_0.detach();
  EXPECT_EQ(thread_0.get_id(), Id());
  thread_1.detach();
  EXPECT_EQ(thread_1.get_id(), Id());
}

TEST(Thread, MoveOperator) {
  Thread thread_0;
  EXPECT_EQ(thread_0.get_id(), Id());

  std::atomic<bool> thread_ran = false;
  Thread thread_1(TestOptionsThread1(), SetBoolTrue, &thread_ran);
  EXPECT_NE(thread_1.get_id(), Id());

  thread_0 = std::move(thread_1);
  EXPECT_NE(thread_0.get_id(), Id());
  EXPECT_EQ(thread_1.get_id(), Id());

  thread_0.detach();
  EXPECT_EQ(thread_0.get_id(), Id());
}

}  // namespace
}  // namespace pw::thread
