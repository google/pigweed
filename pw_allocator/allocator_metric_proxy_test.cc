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

#include "pw_allocator/allocator_metric_proxy.h"

#include "gtest/gtest.h"
#include "pw_allocator_private/allocator_testing.h"

namespace pw::allocator {
namespace {

// Test fixtures.

struct AllocatorMetricProxyTest : ::testing::Test {
 private:
  std::array<std::byte, 256> buffer = {};
  test::FakeAllocator wrapped;

 public:
  AllocatorMetricProxy allocator;

  AllocatorMetricProxyTest() : allocator(0) {}

  void SetUp() override {
    EXPECT_TRUE(wrapped.Initialize(buffer).ok());
    allocator.Initialize(wrapped);
  }
};

// Unit tests.

TEST_F(AllocatorMetricProxyTest, InitiallyZero) {
  EXPECT_EQ(allocator.used(), 0U);
  EXPECT_EQ(allocator.peak(), 0U);
  EXPECT_EQ(allocator.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, MetricsInitialized) {
  auto& memusage = allocator.memusage();
  EXPECT_EQ(memusage.metrics().size(), 3U);
  EXPECT_EQ(memusage.children().size(), 0U);
}

TEST_F(AllocatorMetricProxyTest, AllocateDeallocate) {
  constexpr Layout layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.count(), 1U);

  allocator.Deallocate(ptr, layout);
  EXPECT_EQ(allocator.used(), 0U);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, AllocateFailure) {
  constexpr Layout layout = Layout::Of<uint32_t[0x10000000U]>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(allocator.used(), 0U);
  EXPECT_EQ(allocator.peak(), 0U);
  EXPECT_EQ(allocator.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, AllocateDeallocateMultiple) {
  constexpr Layout layout1 = Layout::Of<uint32_t[3]>();
  void* ptr1 = allocator.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.count(), 1U);

  constexpr Layout layout2 = Layout::Of<uint32_t[2]>();
  void* ptr2 = allocator.Allocate(layout2);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.count(), 2U);

  allocator.Deallocate(ptr1, layout1);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.count(), 1U);

  allocator.Deallocate(ptr2, layout2);
  EXPECT_EQ(allocator.used(), 0U);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, ResizeLarger) {
  constexpr Layout old_layout = Layout::Of<uint32_t[3]>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator.count(), 1U);

  constexpr Layout new_layout = Layout::Of<uint32_t[5]>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.count(), 1U);

  allocator.Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator.used(), 0U);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, ResizeSmaller) {
  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.count(), 1U);

  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator.used(), sizeof(uint32_t));
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.count(), 1U);

  allocator.Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator.used(), 0U);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, Reallocate) {
  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr1 = allocator.Allocate(old_layout);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.count(), 1U);

  // Make a second allocation to force reallocation.
  void* ptr2 = allocator.Allocate(old_layout);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator.count(), 2U);

  // Reallocating allocates before deallocating, leading to higher peaks.
  constexpr Layout new_layout = Layout::Of<uint32_t[4]>();
  void* new_ptr = allocator.Reallocate(ptr1, old_layout, new_layout.size());
  EXPECT_NE(new_ptr, nullptr);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator.count(), 2U);

  allocator.Deallocate(new_ptr, new_layout);
  EXPECT_EQ(allocator.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator.count(), 1U);

  allocator.Deallocate(ptr2, old_layout);
  EXPECT_EQ(allocator.used(), 0U);
  EXPECT_EQ(allocator.peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator.count(), 0U);
}

}  // namespace
}  // namespace pw::allocator
