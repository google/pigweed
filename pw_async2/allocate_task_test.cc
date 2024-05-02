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

#include "pw_async2/allocate_task.h"

#include "pw_allocator/testing.h"

namespace {

using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::AllocateTask;
using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;
using ::pw::async2::Waker;

struct PendableStatus {
  Waker last_waker = {};
  bool should_finish = false;
  int created = 0;
  int polled = 0;
  int destroyed = 0;
};

class Pendable {
 public:
  Pendable(PendableStatus& status) : status_(&status) { ++status_->created; }
  Pendable() = delete;
  Pendable(Pendable&& other) : status_(other.status_) {
    other.status_ = nullptr;
  }
  Pendable& operator=(Pendable&& other) {
    Reset();
    status_ = other.status_;
    other.status_ = nullptr;
    return *this;
  }
  ~Pendable() { Reset(); }
  Poll<> Pend(Context& cx) {
    if (status_ == nullptr) {
      return Pending();
    }
    status_->last_waker = cx.GetWaker(pw::async2::WaitReason::Unspecified());
    ++status_->polled;
    if (status_->should_finish) {
      return Ready();
    }
    return Pending();
  }

 private:
  void Reset() {
    if (status_ != nullptr) {
      ++status_->destroyed;
    }
  }
  PendableStatus* status_;
};

TEST(AllocateTask, AllocatesWithRvalue) {
  AllocatorForTest<256> alloc;
  Dispatcher dispatcher;
  PendableStatus status = {};
  Pendable pendable(status);
  Task* task = AllocateTask(alloc, std::move(pendable));
  ASSERT_NE(task, nullptr);
  EXPECT_NE(alloc.allocate_size(), alloc.deallocate_size());
  task->Destroy();
  EXPECT_EQ(alloc.allocate_size(), alloc.deallocate_size());
}

TEST(AllocateTask, AllocatesWithArgs) {
  AllocatorForTest<256> alloc;
  Dispatcher dispatcher;
  PendableStatus status = {};
  Task* task = AllocateTask<Pendable>(alloc, status);
  ASSERT_NE(task, nullptr);
  EXPECT_NE(alloc.allocate_size(), alloc.deallocate_size());
  task->Destroy();
  EXPECT_EQ(alloc.allocate_size(), alloc.deallocate_size());
}

TEST(AllocateTask, DestroysOnceAfterPendReturnsReady) {
  AllocatorForTest<256> alloc;
  Dispatcher dispatcher;
  PendableStatus status = {};
  Task* task = AllocateTask<Pendable>(alloc, status);
  ASSERT_NE(task, nullptr);
  dispatcher.Post(*task);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(status.polled, 1);
  EXPECT_EQ(status.destroyed, 0);

  std::move(status.last_waker).Wake();
  status.should_finish = true;

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(status.polled, 2);
  EXPECT_EQ(status.destroyed, 1);

  // Ensure that the allocated task is not polled or destroyed again after being
  // deallocated.
  std::move(status.last_waker).Wake();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(status.polled, 2);
  EXPECT_EQ(status.destroyed, 1);
}

}  // namespace
