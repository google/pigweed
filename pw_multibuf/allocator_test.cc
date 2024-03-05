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

#include "pw_multibuf/allocator.h"

#include "gtest/gtest.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/poll.h"

namespace pw::multibuf {
namespace {

using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;

struct AllocateExpectation {
  size_t min_size;
  size_t desired_size;
  bool contiguous;
  pw::Result<MultiBuf> result;
};

class MockMultiBufAllocator : public MultiBufAllocator {
 public:
  ~MockMultiBufAllocator() override {
    // All expectations should have been met and removed.
    EXPECT_EQ(expected_allocate_, std::nullopt);
  }

  void ExpectAllocateAndReturn(size_t min_size,
                               size_t desired_size,
                               bool contiguous,
                               pw::Result<MultiBuf> result) {
    // Multiple simultaneous expectations are not supported.
    ASSERT_FALSE(expected_allocate_.has_value());
    expected_allocate_ = {
        min_size, desired_size, contiguous, std::move(result)};
  }

  using MultiBufAllocator::MoreMemoryAvailable;

 private:
  pw::Result<MultiBuf> DoAllocate(size_t min_size,
                                  size_t desired_size,
                                  bool contiguous) final {
    EXPECT_NE(expected_allocate_, std::nullopt);
    if (!expected_allocate_.has_value()) {
      return Status::FailedPrecondition();
    }
    AllocateExpectation expected = std::move(*expected_allocate_);
    expected_allocate_ = std::nullopt;
    EXPECT_EQ(min_size, expected.min_size);
    EXPECT_EQ(desired_size, expected.desired_size);
    EXPECT_EQ(contiguous, expected.contiguous);
    return std::move(expected.result);
  }

  std::optional<AllocateExpectation> expected_allocate_;
};

class AllocateTask : public Task {
 public:
  AllocateTask(MultiBufAllocationFuture&& future)
      : future_(std::move(future)), last_result_(Pending()) {}

  MultiBufAllocationFuture future_;
  Poll<std::optional<MultiBuf>> last_result_;

 private:
  Poll<> DoPend(Context& cx) override {
    last_result_ = future_.Pend(cx);
    if (last_result_.IsReady()) {
      return Ready();
    }
    return Pending();
  }
};

TEST(MultiBufAllocator, AllocateAsyncReturnsImmediatelyAvailableAllocation) {
  MockMultiBufAllocator alloc;
  AllocateTask task(alloc.AllocateAsync(44, 33));
  alloc.ExpectAllocateAndReturn(44, 33, false, MultiBuf());

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());

  ASSERT_TRUE(task.last_result_.IsReady());
  ASSERT_TRUE(task.last_result_->has_value());
}

TEST(MultiBufAllocator, AllocateAsyncWillNotPollUntilMoreMemoryAvailable) {
  MockMultiBufAllocator alloc;
  AllocateTask task(alloc.AllocateAsync(44, 33));
  Dispatcher dispatcher;
  dispatcher.Post(task);

  // First attempt will return `ResourceExhausted` to signal temporary OOM.
  alloc.ExpectAllocateAndReturn(44, 33, false, Status::ResourceExhausted());
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());
  EXPECT_TRUE(task.last_result_.IsPending());

  // Re-running the dispatcher should not poll the pending task since it has
  // not been awoken. `AllocateAndReturn` should *not* be called.
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());

  // Insufficient memory should not awaken the task.
  alloc.MoreMemoryAvailable(30, 30);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());

  // Sufficient memory will awaken and return the memory
  alloc.MoreMemoryAvailable(50, 50);
  alloc.ExpectAllocateAndReturn(44, 33, false, MultiBuf());
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

}  // namespace
}  // namespace pw::multibuf
