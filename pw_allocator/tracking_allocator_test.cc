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
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

constexpr static size_t kCapacity = 256;

TEST(TrackingAllocatorTest, AutomaticInit_Metrics) {
  WithBuffer<SimpleAllocator, kCapacity> allocator;
  TrackingAllocatorImpl<internal::Metrics> tracker(test::kToken, *allocator);
  EXPECT_TRUE(tracker.initialized());
}

TEST(TrackingAllocatorTest, AutomaticInit_MetricsStub) {
  WithBuffer<SimpleAllocator, kCapacity> allocator;
  TrackingAllocatorImpl<internal::MetricsStub> tracker(test::kToken,
                                                       *allocator);
  EXPECT_TRUE(tracker.initialized());
}

TEST(TrackingAllocatorTest, ManualInit_Metrics) {
  WithBuffer<SimpleAllocator, kCapacity> allocator;
  TrackingAllocatorImpl<internal::Metrics> tracker(test::kToken);
  EXPECT_FALSE(tracker.initialized());

  tracker.Init(*allocator);
  EXPECT_TRUE(tracker.initialized());
}

TEST(TrackingAllocatorTest, ManualInit_MetricsStub) {
  WithBuffer<SimpleAllocator, kCapacity> allocator;
  TrackingAllocatorImpl<internal::MetricsStub> tracker(test::kToken);
  EXPECT_FALSE(tracker.initialized());

  tracker.Init(*allocator);
  EXPECT_TRUE(tracker.initialized());
}

TEST(TrackingAllocatorTest, NoInit_Metrics) {
  WithBuffer<SimpleAllocator, kCapacity> allocator;
  TrackingAllocatorImpl<internal::Metrics> tracker(test::kToken);
  Layout layout = Layout::Of<std::byte>();
  EXPECT_EQ(tracker.Allocate(layout), nullptr);
  tracker.Deallocate(nullptr, layout);
  EXPECT_FALSE(tracker.Resize(nullptr, layout, 2));
  EXPECT_EQ(tracker.Reallocate(nullptr, layout, 2), nullptr);
}

TEST(TrackingAllocatorTest, NoInit_MetricsStub) {
  WithBuffer<SimpleAllocator, kCapacity> allocator;
  TrackingAllocatorImpl<internal::MetricsStub> tracker(test::kToken);
  Layout layout = Layout::Of<std::byte>();
  EXPECT_EQ(tracker.Allocate(layout), nullptr);
  tracker.Deallocate(nullptr, layout);
  EXPECT_FALSE(tracker.Resize(nullptr, layout, 2));
  EXPECT_EQ(tracker.Reallocate(nullptr, layout, 2), nullptr);
}

// These unit tests below use `AllocatorForTest<kBufferSize>`, which forwards
// `Allocator` calls to a `TrackingAllocatorImpl<internal::Metrics>` field. This
// is the same type as `TrackingAllocator`, except that metrics are explicitly
// enabled. As a result, these unit tests do validate `TrackingAllocator`, even
// when that type never appears directly in a test.

TEST(TrackingAllocatorTest, InitialValues) {
  test::AllocatorForTest<kCapacity> allocator;
  EXPECT_EQ(allocator.allocated_bytes(), 0U);
  EXPECT_EQ(allocator.peak_allocated_bytes(), 0U);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), 0U);
  EXPECT_EQ(allocator.num_allocations(), 0U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);
}

TEST(TrackingAllocatorTest, AllocateDeallocate) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  allocator.Deallocate(ptr, layout);
  EXPECT_EQ(allocator.allocated_bytes(), 0U);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 1U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);
}

TEST(TrackingAllocatorTest, AllocateFailure) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout layout = Layout::Of<uint32_t[0x10000000U]>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), 0U);
  EXPECT_EQ(allocator.peak_allocated_bytes(), 0U);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), 0U);
  EXPECT_EQ(allocator.num_allocations(), 0U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 1U);
}

TEST(TrackingAllocatorTest, AllocateDeallocateMultiple) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout layout1 = Layout::Of<uint32_t[3]>();
  void* ptr1 = allocator.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  constexpr Layout layout2 = Layout::Of<uint32_t[2]>();
  void* ptr2 = allocator.Allocate(layout2);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.num_allocations(), 2U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  allocator.Deallocate(ptr1, layout1);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.num_allocations(), 2U);
  EXPECT_EQ(allocator.num_deallocations(), 1U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  constexpr Layout layout3 = Layout::Of<uint32_t>();
  void* ptr3 = allocator.Allocate(layout3);
  ASSERT_NE(ptr3, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator.num_allocations(), 3U);
  EXPECT_EQ(allocator.num_deallocations(), 1U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  allocator.Deallocate(ptr3, layout3);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator.num_allocations(), 3U);
  EXPECT_EQ(allocator.num_deallocations(), 2U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  allocator.Deallocate(ptr2, layout2);
  EXPECT_EQ(allocator.allocated_bytes(), 0U);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator.num_allocations(), 3U);
  EXPECT_EQ(allocator.num_deallocations(), 3U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);
}

TEST(TrackingAllocatorTest, ResizeLarger) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout old_layout = Layout::Of<uint32_t[3]>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  constexpr Layout new_layout = Layout::Of<uint32_t[5]>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 1U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  allocator.Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator.allocated_bytes(), 0U);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 1U);
  EXPECT_EQ(allocator.num_resizes(), 1U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);
}

TEST(TrackingAllocatorTest, ResizeSmaller) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t));
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 1U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  allocator.Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator.allocated_bytes(), 0U);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 1U);
  EXPECT_EQ(allocator.num_resizes(), 1U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);
}

TEST(TrackingAllocatorTest, ResizeFailure) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout layout = Layout::Of<uint32_t[2]>();
  void* ptr1 = allocator.Allocate(layout);
  ASSERT_NE(ptr1, nullptr);
  void* ptr2 = allocator.Allocate(layout);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.num_allocations(), 2U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  EXPECT_FALSE(allocator.Resize(ptr1, layout, layout.size() * 2));
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.num_allocations(), 2U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 1U);
}

TEST(TrackingAllocatorTest, Reallocate) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout layout1 = Layout::Of<uint32_t[2]>();
  void* ptr1 = allocator.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  // If `Reallocate` just resizes, no extra memory is allocated
  constexpr Layout layout2 = Layout::Of<uint32_t[4]>();
  void* new_ptr1 = allocator.Reallocate(ptr1, layout1, layout2.size());
  EXPECT_EQ(new_ptr1, ptr1);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 1U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  // Make a second allocation to force reallocation.
  void* ptr2 = allocator.Allocate(layout1);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator.num_allocations(), 2U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 1U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  // If `Reallocate` must copy to a new location, it allocates before
  // deallocating and results in higher peaks.
  constexpr Layout layout3 = Layout::Of<uint32_t[8]>();
  new_ptr1 = allocator.Reallocate(ptr1, layout2, layout3.size());
  EXPECT_NE(new_ptr1, ptr1);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 10);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 14);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 14);
  EXPECT_EQ(allocator.num_allocations(), 2U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 2U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  allocator.Deallocate(ptr2, layout1);
  allocator.Deallocate(new_ptr1, layout3);
  EXPECT_EQ(allocator.allocated_bytes(), 0U);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 14);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 14);
  EXPECT_EQ(allocator.num_allocations(), 2U);
  EXPECT_EQ(allocator.num_deallocations(), 2U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 2U);
  EXPECT_EQ(allocator.num_failures(), 0U);
}

TEST(TrackingAllocatorTest, ReallocateFailure) {
  test::AllocatorForTest<kCapacity> allocator;
  constexpr Layout layout = Layout::Of<uint32_t[4]>();
  void* ptr1 = allocator.Allocate(layout);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 0U);

  void* ptr2 = allocator.Reallocate(ptr1, layout, 0x10000000U);
  EXPECT_EQ(ptr2, nullptr);
  EXPECT_EQ(allocator.allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.peak_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.cumulative_allocated_bytes(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.num_allocations(), 1U);
  EXPECT_EQ(allocator.num_deallocations(), 0U);
  EXPECT_EQ(allocator.num_resizes(), 0U);
  EXPECT_EQ(allocator.num_reallocations(), 0U);
  EXPECT_EQ(allocator.num_failures(), 1U);
}

}  // namespace
}  // namespace pw::allocator
