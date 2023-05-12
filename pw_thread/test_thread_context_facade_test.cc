// Copyright 2023 The Pigweed Authors
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

#include "gtest/gtest.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"

using pw::thread::test::TestThreadContext;

namespace pw::thread {
namespace {

static void ReleaseBinarySemaphore(void* arg) {
  static_cast<sync::BinarySemaphore*>(arg)->release();
}

TEST(Thread, TestThreadContext) {
  TestThreadContext context_0;
  TestThreadContext context_1;
  Thread thread_0;
  Thread thread_1;
  sync::BinarySemaphore thread_ran_sem_0;
  sync::BinarySemaphore thread_ran_sem_1;

  thread_0 =
      Thread(context_0.options(), ReleaseBinarySemaphore, &thread_ran_sem_0);
  thread_1 =
      Thread(context_1.options(), ReleaseBinarySemaphore, &thread_ran_sem_1);

  EXPECT_NE(thread_0.get_id(), Id());
  EXPECT_TRUE(thread_0.joinable());

  EXPECT_NE(thread_1.get_id(), Id());
  EXPECT_TRUE(thread_1.joinable());

  thread_0.detach();
  thread_1.detach();

  EXPECT_EQ(thread_0.get_id(), Id());
  EXPECT_FALSE(thread_0.joinable());

  EXPECT_EQ(thread_1.get_id(), Id());
  EXPECT_FALSE(thread_1.joinable());

  thread_ran_sem_0.acquire();
  thread_ran_sem_1.acquire();
}

}  // namespace
}  // namespace pw::thread
