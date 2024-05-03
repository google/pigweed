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

#include <cstdint>

#include "pw_allocator/allocator.h"
#include "pw_allocator/metrics.h"
#include "pw_allocator/testing.h"
#include "pw_log/log.h"
#include "pw_metric/metric.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

using ::pw::allocator::Layout;
using ::pw::allocator::NoMetrics;
using ::pw::allocator::TrackingAllocator;
using TestMetrics = ::pw::allocator::internal::AllMetrics;

class TrackingAllocatorForTest : public TrackingAllocator<TestMetrics> {
 public:
  TrackingAllocatorForTest(pw::metric::Token token, pw::Allocator& allocator)
      : TrackingAllocator<TestMetrics>(token, allocator) {}

  // Expose the protected ``GetAllocatedLayout`` method for test purposes.
  pw::Result<Layout> GetAllocatedLayout(const void* ptr) const {
    return Allocator::GetAllocatedLayout(*this, ptr);
  }
};

class TrackingAllocatorTest : public ::testing::Test {
 protected:
  using AllocatorType = ::pw::allocator::FirstFitBlockAllocator<uint32_t>;
  using BlockType = AllocatorType::BlockType;

  constexpr static size_t kCapacity = 256;
  constexpr static pw::metric::Token kToken = 1U;

  TrackingAllocatorTest() : ::testing::Test(), tracker_(kToken, *allocator_) {}

  void SetUp() override {
    EXPECT_EQ(allocator_->Init(allocator_.as_bytes()), pw::OkStatus());
  }

  void TearDown() override {
    for (auto* block : allocator_->blocks()) {
      BlockType::Free(block);
    }
    allocator_->Reset();
  }

  pw::allocator::WithBuffer<AllocatorType, kCapacity, BlockType::kAlignment>
      allocator_;
  TrackingAllocatorForTest tracker_;
};

struct ExpectedValues {
  uint32_t requested_bytes = 0;
  uint32_t peak_requested_bytes = 0;
  uint32_t cumulative_requested_bytes = 0;
  uint32_t allocated_bytes = 0;
  uint32_t peak_allocated_bytes = 0;
  uint32_t cumulative_allocated_bytes = 0;
  uint32_t num_allocations = 0;
  uint32_t num_deallocations = 0;
  uint32_t num_resizes = 0;
  uint32_t num_reallocations = 0;
  uint32_t num_failures = 0;
  uint32_t unfulfilled_bytes = 0;

  void AddRequestedBytes(uint32_t requested_bytes_) {
    requested_bytes += requested_bytes_;
    peak_requested_bytes = std::max(requested_bytes, peak_requested_bytes);
    cumulative_requested_bytes += requested_bytes_;
  }

  void AddAllocatedBytes(uint32_t allocated_bytes_) {
    allocated_bytes += allocated_bytes_;
    peak_allocated_bytes = std::max(allocated_bytes, peak_allocated_bytes);
    cumulative_allocated_bytes += allocated_bytes_;
  }

  void Check(const TestMetrics& metrics, int line) {
    EXPECT_EQ(metrics.requested_bytes.value(), requested_bytes);
    EXPECT_EQ(metrics.peak_requested_bytes.value(), peak_requested_bytes);
    EXPECT_EQ(metrics.cumulative_requested_bytes.value(),
              cumulative_requested_bytes);
    EXPECT_EQ(metrics.allocated_bytes.value(), allocated_bytes);
    EXPECT_EQ(metrics.peak_allocated_bytes.value(), peak_allocated_bytes);
    EXPECT_EQ(metrics.cumulative_allocated_bytes.value(),
              cumulative_allocated_bytes);
    EXPECT_EQ(metrics.num_allocations.value(), num_allocations);
    EXPECT_EQ(metrics.num_deallocations.value(), num_deallocations);
    EXPECT_EQ(metrics.num_resizes.value(), num_resizes);
    EXPECT_EQ(metrics.num_reallocations.value(), num_reallocations);
    EXPECT_EQ(metrics.num_failures.value(), num_failures);
    EXPECT_EQ(metrics.unfulfilled_bytes.value(), unfulfilled_bytes);
    if (testing::Test::HasFailure()) {
      PW_LOG_ERROR("Metrics comparison failed at line %d.", line);
    }
  }
};

#define EXPECT_METRICS_EQ(expected, metrics) expected.Check(metrics, __LINE__)

// Unit tests.

TEST_F(TrackingAllocatorTest, InitialValues) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;  // Initially all 0.
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, GetCapacity) {
  pw::StatusWithSize capacity = tracker_.GetCapacity();
  EXPECT_EQ(capacity.status(), pw::OkStatus());
  EXPECT_EQ(capacity.size(), kCapacity);
}

TEST_F(TrackingAllocatorTest, AddTrackingAllocatorAsChild) {
  constexpr static pw::metric::Token kChildToken = 2U;
  TrackingAllocator<NoMetrics> child(
      kChildToken, tracker_, pw::allocator::kAddTrackingAllocatorAsChild);
  pw::IntrusiveList<pw::metric::Group>& children =
      tracker_.metric_group().children();
  ASSERT_FALSE(children.empty());
  EXPECT_EQ(children.size(), 1U);
  EXPECT_EQ(&(children.front()), &(child.metric_group()));
}

TEST_F(TrackingAllocatorTest, AllocateDeallocate) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  constexpr Layout layout1 = Layout::Of<uint32_t[2]>();
  void* ptr1 = tracker_.Allocate(layout1);
  size_t ptr1_allocated = tracker_.GetAllocatedLayout(ptr1)->size();
  ASSERT_NE(ptr1, nullptr);
  expected.AddRequestedBytes(layout1.size());
  expected.AddAllocatedBytes(ptr1_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr1);
  expected.requested_bytes -= layout1.size();
  expected.allocated_bytes -= ptr1_allocated;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, AllocateFailure) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  constexpr Layout layout = Layout::Of<uint32_t[0x10000000U]>();
  void* ptr = tracker_.Allocate(layout);
  EXPECT_EQ(ptr, nullptr);
  expected.num_failures += 1;
  expected.unfulfilled_bytes += layout.size();
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, AllocateDeallocateMultiple) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  Layout layout1 = Layout::Of<uint32_t[3]>();
  void* ptr1 = tracker_.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  size_t ptr1_allocated = tracker_.GetAllocatedLayout(ptr1)->size();
  expected.AddRequestedBytes(layout1.size());
  expected.AddAllocatedBytes(ptr1_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  Layout layout2 = Layout::Of<uint32_t[2]>();
  void* ptr2 = tracker_.Allocate(layout2);
  ASSERT_NE(ptr2, nullptr);
  size_t ptr2_allocated = tracker_.GetAllocatedLayout(ptr2)->size();
  expected.AddRequestedBytes(layout2.size());
  expected.AddAllocatedBytes(ptr2_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr1);
  expected.requested_bytes -= layout1.size();
  expected.allocated_bytes -= ptr1_allocated;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  Layout layout3 = Layout::Of<uint32_t>();
  void* ptr3 = tracker_.Allocate(layout3);
  ASSERT_NE(ptr3, nullptr);
  size_t ptr3_allocated = tracker_.GetAllocatedLayout(ptr3)->size();
  expected.AddRequestedBytes(layout3.size());
  expected.AddAllocatedBytes(ptr3_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr3);
  expected.requested_bytes -= layout3.size();
  expected.allocated_bytes -= ptr3_allocated;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr2);
  expected.requested_bytes -= layout2.size();
  expected.allocated_bytes -= ptr2_allocated;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, ResizeLarger) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  constexpr Layout layout1 = Layout::Of<uint32_t[3]>();
  void* ptr = tracker_.Allocate(layout1);
  size_t ptr_allocated1 = tracker_.GetAllocatedLayout(ptr)->size();
  ASSERT_NE(ptr, nullptr);
  expected.AddRequestedBytes(layout1.size());
  expected.AddAllocatedBytes(ptr_allocated1);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  constexpr size_t size2 = sizeof(uint32_t[5]);
  EXPECT_TRUE(tracker_.Resize(ptr, size2));
  size_t ptr_allocated2 = tracker_.GetAllocatedLayout(ptr)->size();
  expected.AddRequestedBytes(size2 - layout1.size());
  expected.AddAllocatedBytes(ptr_allocated2 - ptr_allocated1);
  expected.num_resizes += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr);
  expected.requested_bytes -= size2;
  expected.allocated_bytes -= ptr_allocated2;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, ResizeSmaller) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  constexpr Layout layout1 = Layout::Of<uint32_t[2]>();
  void* ptr = tracker_.Allocate(layout1);
  size_t ptr_allocated1 = tracker_.GetAllocatedLayout(ptr)->size();
  ASSERT_NE(ptr, nullptr);
  expected.AddRequestedBytes(layout1.size());
  expected.AddAllocatedBytes(ptr_allocated1);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  constexpr size_t size2 = sizeof(uint32_t[1]);
  EXPECT_TRUE(tracker_.Resize(ptr, size2));
  size_t ptr_allocated2 = tracker_.GetAllocatedLayout(ptr)->size();
  expected.requested_bytes -= layout1.size() - size2;
  expected.allocated_bytes -= ptr_allocated1 - ptr_allocated2;
  expected.num_resizes += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr);
  expected.requested_bytes -= size2;
  expected.allocated_bytes -= ptr_allocated2;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, ResizeFailure) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  constexpr Layout layout = Layout::Of<uint32_t[4]>();
  void* ptr1 = tracker_.Allocate(layout);
  ASSERT_NE(ptr1, nullptr);
  size_t ptr1_allocated = tracker_.GetAllocatedLayout(ptr1)->size();
  expected.AddRequestedBytes(layout.size());
  expected.AddAllocatedBytes(ptr1_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  void* ptr2 = tracker_.Allocate(layout);
  ASSERT_NE(ptr2, nullptr);
  size_t ptr2_allocated = tracker_.GetAllocatedLayout(ptr2)->size();
  expected.AddRequestedBytes(layout.size());
  expected.AddAllocatedBytes(ptr2_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  EXPECT_FALSE(tracker_.Resize(ptr1, layout.size() * 2));
  expected.num_failures += 1;
  expected.unfulfilled_bytes += layout.size() * 2;
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, Reallocate) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  constexpr Layout layout1 = Layout::Of<uint32_t[2]>();
  void* ptr1 = tracker_.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  size_t ptr1_allocated = tracker_.GetAllocatedLayout(ptr1)->size();
  expected.AddRequestedBytes(layout1.size());
  expected.AddAllocatedBytes(ptr1_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  // If `Reallocate` just resizes, no extra memory is allocated
  constexpr Layout layout2 = Layout::Of<uint32_t[4]>();
  void* ptr2 = tracker_.Reallocate(ptr1, layout2);
  EXPECT_EQ(ptr2, ptr1);
  size_t ptr2_allocated = tracker_.GetAllocatedLayout(ptr2)->size();
  expected.AddRequestedBytes(layout2.size() - layout1.size());
  expected.AddAllocatedBytes(ptr2_allocated - ptr1_allocated);
  expected.num_reallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  // Make a second allocation to force reallocation.
  constexpr Layout layout3 = layout1;
  void* ptr3 = tracker_.Allocate(layout1);
  ASSERT_NE(ptr3, nullptr);
  size_t ptr3_allocated = tracker_.GetAllocatedLayout(ptr3)->size();
  expected.AddRequestedBytes(layout3.size());
  expected.AddAllocatedBytes(ptr3_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  // If `Reallocate` must copy to a new location, it allocates before
  // deallocating and results in higher peaks.
  constexpr Layout layout4 = Layout::Of<uint32_t[8]>();
  void* ptr4 = tracker_.Reallocate(ptr2, layout4);
  EXPECT_NE(ptr4, ptr2);
  size_t ptr4_allocated = tracker_.GetAllocatedLayout(ptr4)->size();
  expected.AddRequestedBytes(layout4.size() - layout2.size());
  expected.AddAllocatedBytes(ptr4_allocated);
  expected.allocated_bytes -= ptr2_allocated;
  expected.num_reallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr3);
  expected.requested_bytes -= layout3.size();
  expected.allocated_bytes -= ptr3_allocated;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  tracker_.Deallocate(ptr4);
  expected.requested_bytes -= layout4.size();
  expected.allocated_bytes -= ptr4_allocated;
  expected.num_deallocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);
}

TEST_F(TrackingAllocatorTest, ReallocateFailure) {
  const TestMetrics& metrics = tracker_.metrics();
  ExpectedValues expected;

  constexpr Layout layout1 = Layout::Of<uint32_t[4]>();
  void* ptr1 = tracker_.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  size_t ptr1_allocated = tracker_.GetAllocatedLayout(ptr1)->size();
  expected.AddRequestedBytes(layout1.size());
  expected.AddAllocatedBytes(ptr1_allocated);
  expected.num_allocations += 1;
  EXPECT_METRICS_EQ(expected, metrics);

  constexpr Layout layout2 = Layout(0x10000000U, 1);
  void* ptr2 = tracker_.Reallocate(ptr1, layout2);
  EXPECT_EQ(ptr2, nullptr);
  expected.num_failures += 1;
  expected.unfulfilled_bytes += layout2.size();
  EXPECT_METRICS_EQ(expected, metrics);
}

}  // namespace
