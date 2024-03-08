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
#include "pw_allocator/tracking_allocator.h"

#include "pw_allocator/allocator_testing.h"
#include "pw_allocator/block_allocator.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

// Test fixture.
class TrackingAllocatorTest : public ::testing::Test {
 protected:
  using AllocatorType = FirstFitBlockAllocator<uint32_t>;
  using BlockType = AllocatorType::BlockType;

  constexpr static size_t kCapacity = 256;
  constexpr static metric::Token kToken = 1U;

  TrackingAllocatorTest() : ::testing::Test(), tracker_(kToken, *allocator_) {}

  void SetUp() override {
    EXPECT_EQ(allocator_->Init(allocator_.as_bytes()), OkStatus());
  }

  void TearDown() override {
    for (auto* block : allocator_->blocks()) {
      BlockType::Free(block);
    }
    allocator_->Reset();
  }

  WithBuffer<AllocatorType, kCapacity> allocator_;
  TrackingAllocatorImpl<AllMetrics> tracker_;
};

// Unit tests.

TEST_F(TrackingAllocatorTest, InitialValues) {
  const AllMetrics& metrics = tracker_.metrics();

  EXPECT_EQ(metrics.allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.num_allocations.value(), 0U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);
}

TEST_F(TrackingAllocatorTest, AddTrackingAllocatorAsChild) {
  constexpr static metric::Token kChildToken = 2U;
  TrackingAllocatorImpl<NoMetrics> child(
      kChildToken, tracker_, kAddTrackingAllocatorAsChild);
  IntrusiveList<metric::Group>& children = tracker_.metric_group().children();
  ASSERT_FALSE(children.empty());
  EXPECT_EQ(children.size(), 1U);
  EXPECT_EQ(&(children.front()), &(child.metric_group()));
}

TEST_F(TrackingAllocatorTest, AllocateDeallocate) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout layout = Layout::Of<uint32_t[2]>();
  void* ptr = tracker_.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  tracker_.Deallocate(ptr, layout);
  EXPECT_EQ(metrics.allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 1U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);
}

TEST_F(TrackingAllocatorTest, AllocateFailure) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout layout = Layout::Of<uint32_t[0x10000000U]>();
  void* ptr = tracker_.Allocate(layout);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.num_allocations.value(), 0U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 1U);
}

TEST_F(TrackingAllocatorTest, AllocateDeallocateMultiple) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout layout1 = Layout::Of<uint32_t[3]>();
  void* ptr1 = tracker_.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 3);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 3);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 3);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  constexpr Layout layout2 = Layout::Of<uint32_t[2]>();
  void* ptr2 = tracker_.Allocate(layout2);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  tracker_.Deallocate(ptr1, layout1);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  EXPECT_EQ(metrics.num_deallocations.value(), 1U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  constexpr Layout layout3 = Layout::Of<uint32_t>();
  void* ptr3 = tracker_.Allocate(layout3);
  ASSERT_NE(ptr3, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 3);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 6);
  EXPECT_EQ(metrics.num_allocations.value(), 3U);
  EXPECT_EQ(metrics.num_deallocations.value(), 1U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  tracker_.Deallocate(ptr3, layout3);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 6);
  EXPECT_EQ(metrics.num_allocations.value(), 3U);
  EXPECT_EQ(metrics.num_deallocations.value(), 2U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  tracker_.Deallocate(ptr2, layout2);
  EXPECT_EQ(metrics.allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 6);
  EXPECT_EQ(metrics.num_allocations.value(), 3U);
  EXPECT_EQ(metrics.num_deallocations.value(), 3U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);
}

TEST_F(TrackingAllocatorTest, ResizeLarger) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout old_layout = Layout::Of<uint32_t[3]>();
  void* ptr = tracker_.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 3);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 3);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 3);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  constexpr Layout new_layout = Layout::Of<uint32_t[5]>();
  EXPECT_TRUE(tracker_.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 1U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  tracker_.Deallocate(ptr, new_layout);
  EXPECT_EQ(metrics.allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 5);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 1U);
  EXPECT_EQ(metrics.num_resizes.value(), 1U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);
}

TEST_F(TrackingAllocatorTest, ResizeSmaller) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr = tracker_.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(tracker_.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t));
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 1U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  tracker_.Deallocate(ptr, new_layout);
  EXPECT_EQ(metrics.allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 1U);
  EXPECT_EQ(metrics.num_resizes.value(), 1U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);
}

TEST_F(TrackingAllocatorTest, ResizeFailure) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout layout = Layout::Of<uint32_t[2]>();
  void* ptr1 = tracker_.Allocate(layout);
  ASSERT_NE(ptr1, nullptr);
  void* ptr2 = tracker_.Allocate(layout);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  EXPECT_FALSE(tracker_.Resize(ptr1, layout, layout.size() * 2));
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 1U);
}

TEST_F(TrackingAllocatorTest, Reallocate) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout layout1 = Layout::Of<uint32_t[2]>();
  void* ptr1 = tracker_.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 2);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  // If `Reallocate` just resizes, no extra memory is allocated
  constexpr Layout layout2 = Layout::Of<uint32_t[4]>();
  void* new_ptr1 = tracker_.Reallocate(ptr1, layout1, layout2.size());
  EXPECT_EQ(new_ptr1, ptr1);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 1U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  // Make a second allocation to force reallocation.
  void* ptr2 = tracker_.Allocate(layout1);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 6);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 6);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 6);
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 1U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  // If `Reallocate` must copy to a new location, it allocates before
  // deallocating and results in higher peaks.
  constexpr Layout layout3 = Layout::Of<uint32_t[8]>();
  new_ptr1 = tracker_.Reallocate(ptr1, layout2, layout3.size());
  EXPECT_NE(new_ptr1, ptr1);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 10);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 14);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 14);
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 2U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  tracker_.Deallocate(ptr2, layout1);
  tracker_.Deallocate(new_ptr1, layout3);
  EXPECT_EQ(metrics.allocated_bytes.value(), 0U);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 14);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 14);
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  EXPECT_EQ(metrics.num_deallocations.value(), 2U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 2U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);
}

TEST_F(TrackingAllocatorTest, ReallocateFailure) {
  const AllMetrics& metrics = tracker_.metrics();

  constexpr Layout layout = Layout::Of<uint32_t[4]>();
  void* ptr1 = tracker_.Allocate(layout);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 0U);

  void* ptr2 = tracker_.Reallocate(ptr1, layout, 0x10000000U);
  EXPECT_EQ(ptr2, nullptr);
  EXPECT_EQ(metrics.allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.peak_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.cumulative_allocated_bytes.value(), sizeof(uint32_t) * 4);
  EXPECT_EQ(metrics.num_allocations.value(), 1U);
  EXPECT_EQ(metrics.num_deallocations.value(), 0U);
  EXPECT_EQ(metrics.num_resizes.value(), 0U);
  EXPECT_EQ(metrics.num_reallocations.value(), 0U);
  EXPECT_EQ(metrics.num_failures.value(), 1U);
}

}  // namespace
}  // namespace pw::allocator
