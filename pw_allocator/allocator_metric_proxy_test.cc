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
#include "pw_allocator/allocator_metric_proxy_for_test.h"
#include "pw_allocator/allocator_testing.h"

namespace pw::allocator {
namespace {

// Test fixtures.

class AllocatorMetricProxyTest : public ::testing::Test {
 protected:
  AllocatorMetricProxyTest() : allocator_(0) {}

  void SetUp() override { allocator_.Init(*wrapped); }

  AllocatorMetricProxyForTest allocator_;

 private:
  test::AllocatorForTestWithBuffer<256> wrapped;
};

// Unit tests.

TEST_F(AllocatorMetricProxyTest, InitiallyZero) {
  EXPECT_EQ(allocator_.used(), 0U);
  EXPECT_EQ(allocator_.peak(), 0U);
  EXPECT_EQ(allocator_.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, MetricsInitialized) {
  EXPECT_EQ(allocator_.metrics().size(), 3U);
  EXPECT_EQ(allocator_.children().size(), 0U);
}

TEST_F(AllocatorMetricProxyTest, AllocateDeallocate) {
  constexpr Layout layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator_.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.count(), 1U);

  allocator_.Deallocate(ptr, layout);
  EXPECT_EQ(allocator_.used(), 0U);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, AllocateFailure) {
  constexpr Layout layout = Layout::Of<uint32_t[0x10000000U]>();
  void* ptr = allocator_.Allocate(layout);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(allocator_.used(), 0U);
  EXPECT_EQ(allocator_.peak(), 0U);
  EXPECT_EQ(allocator_.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, AllocateDeallocateMultiple) {
  constexpr Layout layout1 = Layout::Of<uint32_t[3]>();
  void* ptr1 = allocator_.Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator_.count(), 1U);

  constexpr Layout layout2 = Layout::Of<uint32_t[2]>();
  void* ptr2 = allocator_.Allocate(layout2);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator_.count(), 2U);

  allocator_.Deallocate(ptr1, layout1);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator_.count(), 1U);

  allocator_.Deallocate(ptr2, layout2);
  EXPECT_EQ(allocator_.used(), 0U);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator_.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, ResizeLarger) {
  constexpr Layout old_layout = Layout::Of<uint32_t[3]>();
  void* ptr = allocator_.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator_.count(), 1U);

  constexpr Layout new_layout = Layout::Of<uint32_t[5]>();
  EXPECT_TRUE(allocator_.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator_.count(), 1U);

  allocator_.Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator_.used(), 0U);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator_.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, ResizeSmaller) {
  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator_.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.count(), 1U);

  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(allocator_.Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t));
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.count(), 1U);

  allocator_.Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator_.used(), 0U);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.count(), 0U);
}

TEST_F(AllocatorMetricProxyTest, Reallocate) {
  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr1 = allocator_.Allocate(old_layout);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.count(), 1U);

  // Make a second allocation to force reallocation.
  void* ptr2 = allocator_.Allocate(old_layout);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator_.count(), 2U);

  // Reallocating allocates before deallocating, leading to higher peaks.
  constexpr Layout new_layout = Layout::Of<uint32_t[4]>();
  void* new_ptr = allocator_.Reallocate(ptr1, old_layout, new_layout.size());
  EXPECT_NE(new_ptr, nullptr);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator_.count(), 2U);

  allocator_.Deallocate(new_ptr, new_layout);
  EXPECT_EQ(allocator_.used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator_.count(), 1U);

  allocator_.Deallocate(ptr2, old_layout);
  EXPECT_EQ(allocator_.used(), 0U);
  EXPECT_EQ(allocator_.peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator_.count(), 0U);
}

}  // namespace
}  // namespace pw::allocator
