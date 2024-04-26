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

#include "pw_sync/binary_semaphore.h"
#include "pw_thread/id.h"
#include "pw_thread/non_portable_test_thread_options.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::sync::BinarySemaphore;
using ::pw::thread::Id;
using ::pw::thread::Thread;
using ::pw::thread::ThreadCore;
using ::pw::thread::test::TestOptionsThread0;
using ::pw::thread::test::TestOptionsThread1;
using ::pw::thread::test::WaitUntilDetachedThreadsCleanedUp;

TEST(Thread, DefaultIds) {
  Thread not_executing_thread;
  EXPECT_EQ(not_executing_thread.get_id(), Id());
}

#if PW_THREAD_JOINING_ENABLED
TEST(Thread, DefaultConstructedThreadIsNotJoinable) {
  Thread thread;
  EXPECT_FALSE(thread.joinable());
}

TEST(Thread, JoinWaitsForLambdaCompletion) {
  BinarySemaphore thread_ran;
  Thread thread(TestOptionsThread0(), [&thread_ran] { thread_ran.release(); });
  EXPECT_TRUE(thread.joinable());
  thread.join();
  EXPECT_EQ(thread.get_id(), Id());
  EXPECT_TRUE(thread_ran.try_acquire());
}

static void ReleaseBinarySemaphore(void* arg) {
  static_cast<BinarySemaphore*>(arg)->release();
}

TEST(Thread, JoinWaitsForDeprecatedFunctionPointerCompletion) {
  BinarySemaphore thread_ran;
  Thread thread(TestOptionsThread0(), ReleaseBinarySemaphore, &thread_ran);
  EXPECT_TRUE(thread.joinable());
  thread.join();
  EXPECT_EQ(thread.get_id(), Id());
  EXPECT_TRUE(thread_ran.try_acquire());
}
#endif  // PW_THREAD_JOINING_ENABLED

TEST(Thread, DetachAllowsThreadToRunAfterExitingScope) {
  struct {
    BinarySemaphore thread_blocker;
    BinarySemaphore thread_finished;
  } semaphores;
  {
    Thread thread(TestOptionsThread0(), [&semaphores] {
      semaphores.thread_blocker.acquire();
      semaphores.thread_finished.release();
    });
    EXPECT_NE(thread.get_id(), Id());
    EXPECT_TRUE(thread.joinable());
    thread.detach();
    EXPECT_EQ(thread.get_id(), Id());
    EXPECT_FALSE(thread.joinable());
  }
  EXPECT_FALSE(semaphores.thread_finished.try_acquire());
  semaphores.thread_blocker.release();
  semaphores.thread_finished.acquire();

  WaitUntilDetachedThreadsCleanedUp();
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

  BinarySemaphore thread_ran_sem;
  Thread thread_1(TestOptionsThread1(),
                  [&thread_ran_sem] { thread_ran_sem.release(); });

  EXPECT_NE(thread_1.get_id(), Id());

  thread_0.swap(thread_1);
  EXPECT_NE(thread_0.get_id(), Id());
  EXPECT_EQ(thread_1.get_id(), Id());

  thread_0.detach();
  EXPECT_EQ(thread_0.get_id(), Id());

  thread_ran_sem.acquire();
  WaitUntilDetachedThreadsCleanedUp();
}

TEST(Thread, SwapWithTwoExecuting) {
  BinarySemaphore thread_a_ran_sem;
  Thread thread_0(TestOptionsThread0(),
                  [&thread_a_ran_sem] { thread_a_ran_sem.release(); });
  BinarySemaphore thread_b_ran_sem;
  Thread thread_1(TestOptionsThread1(),
                  [&thread_b_ran_sem] { thread_b_ran_sem.release(); });
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

  thread_a_ran_sem.acquire();
  thread_b_ran_sem.acquire();
  WaitUntilDetachedThreadsCleanedUp();
}

TEST(Thread, MoveOperator) {
  Thread thread_0;
  EXPECT_EQ(thread_0.get_id(), Id());

  BinarySemaphore thread_ran_sem;
  Thread thread_1(TestOptionsThread1(),
                  [&thread_ran_sem] { thread_ran_sem.release(); });
  EXPECT_NE(thread_1.get_id(), Id());

  thread_0 = std::move(thread_1);
  EXPECT_NE(thread_0.get_id(), Id());
#ifndef __clang_analyzer__
  EXPECT_EQ(thread_1.get_id(), Id());
#endif  // ignore use-after-move

  thread_0.detach();
  EXPECT_EQ(thread_0.get_id(), Id());

  thread_ran_sem.acquire();
  WaitUntilDetachedThreadsCleanedUp();
}

class SemaphoreReleaser : public ThreadCore {
 public:
  BinarySemaphore& semaphore() { return semaphore_; }

 private:
  void Run() override { semaphore_.release(); }

  BinarySemaphore semaphore_;
};

TEST(Thread, ThreadCore) {
  SemaphoreReleaser semaphore_releaser;
  Thread thread(TestOptionsThread0(), semaphore_releaser);
  EXPECT_NE(thread.get_id(), Id());
  EXPECT_TRUE(thread.joinable());
  thread.detach();
  EXPECT_EQ(thread.get_id(), Id());
  EXPECT_FALSE(thread.joinable());
  semaphore_releaser.semaphore().acquire();

  WaitUntilDetachedThreadsCleanedUp();
}

}  // namespace
