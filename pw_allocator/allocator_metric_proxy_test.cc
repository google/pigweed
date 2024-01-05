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

#include "pw_allocator/allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

// These unit tests below use `AllocatorForTest<kBufferSize>`, which forwards
// `Allocator` calls to a field of type
// `AllocatorMetricProxyImpl<internal::Metrics>`. This is the same type as
// `AllocatorMetricProxy`, except that metrics are explicitly enabled. As a
// result, these unit tests do validate `AllocatorMetricProxy`, even when that
// type never appears directly in a test.

TEST(AllocatorMetricProxyTest, ExplicitlyInitialized) {
  WithBuffer<SimpleAllocator, 256> allocator;
  AllocatorMetricProxyImpl<internal::Metrics> proxy(test::kToken);

  metric::Group& group = proxy.metric_group();
  EXPECT_EQ(group.metrics().size(), 0U);
  EXPECT_EQ(group.children().size(), 0U);

  proxy.Init(*allocator);
  EXPECT_EQ(group.metrics().size(), 3U);
  EXPECT_EQ(group.children().size(), 0U);
}

TEST(AllocatorMetricProxyTest, AutomaticallyInitialized) {
  WithBuffer<SimpleAllocator, 256> allocator;
  AllocatorMetricProxyImpl<internal::Metrics> proxy(test::kToken, *allocator);

  metric::Group& group = proxy.metric_group();
  EXPECT_EQ(group.metrics().size(), 3U);
  EXPECT_EQ(group.children().size(), 0U);
}

TEST(AllocatorMetricProxyTest, InitiallyZero) {
  test::AllocatorForTest<256> allocator;
  EXPECT_EQ(allocator->used(), 0U);
  EXPECT_EQ(allocator->peak(), 0U);
  EXPECT_EQ(allocator->count(), 0U);
}

TEST(AllocatorMetricProxyTest, AllocateDeallocate) {
  test::AllocatorForTest<256> allocator;
  constexpr Layout layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator->Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->count(), 1U);

  allocator->Deallocate(ptr, layout);
  EXPECT_EQ(allocator->used(), 0U);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->count(), 0U);
}

TEST(AllocatorMetricProxyTest, AllocateFailure) {
  test::AllocatorForTest<256> allocator;
  constexpr Layout layout = Layout::Of<uint32_t[0x10000000U]>();
  void* ptr = allocator->Allocate(layout);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(allocator->used(), 0U);
  EXPECT_EQ(allocator->peak(), 0U);
  EXPECT_EQ(allocator->count(), 0U);
}

TEST(AllocatorMetricProxyTest, AllocateDeallocateMultiple) {
  test::AllocatorForTest<256> allocator;
  constexpr Layout layout1 = Layout::Of<uint32_t[3]>();
  void* ptr1 = allocator->Allocate(layout1);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator->count(), 1U);

  constexpr Layout layout2 = Layout::Of<uint32_t[2]>();
  void* ptr2 = allocator->Allocate(layout2);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator->count(), 2U);

  allocator->Deallocate(ptr1, layout1);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator->count(), 1U);

  allocator->Deallocate(ptr2, layout2);
  EXPECT_EQ(allocator->used(), 0U);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator->count(), 0U);
}

TEST(AllocatorMetricProxyTest, ResizeLarger) {
  test::AllocatorForTest<256> allocator;
  constexpr Layout old_layout = Layout::Of<uint32_t[3]>();
  void* ptr = allocator->Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 3);
  EXPECT_EQ(allocator->count(), 1U);

  constexpr Layout new_layout = Layout::Of<uint32_t[5]>();
  EXPECT_TRUE(allocator->Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator->count(), 1U);

  allocator->Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator->used(), 0U);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 5);
  EXPECT_EQ(allocator->count(), 0U);
}

TEST(AllocatorMetricProxyTest, ResizeSmaller) {
  test::AllocatorForTest<256> allocator;
  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr = allocator->Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->count(), 1U);

  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(allocator->Resize(ptr, old_layout, new_layout.size()));
  EXPECT_EQ(allocator->used(), sizeof(uint32_t));
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->count(), 1U);

  allocator->Deallocate(ptr, new_layout);
  EXPECT_EQ(allocator->used(), 0U);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->count(), 0U);
}

TEST(AllocatorMetricProxyTest, Reallocate) {
  test::AllocatorForTest<256> allocator;
  constexpr Layout old_layout = Layout::Of<uint32_t[2]>();
  void* ptr1 = allocator->Allocate(old_layout);
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->count(), 1U);

  // Make a second allocation to force reallocation.
  void* ptr2 = allocator->Allocate(old_layout);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 4);
  EXPECT_EQ(allocator->count(), 2U);

  // Reallocating allocates before deallocating, leading to higher peaks.
  constexpr Layout new_layout = Layout::Of<uint32_t[4]>();
  void* new_ptr = allocator->Reallocate(ptr1, old_layout, new_layout.size());
  EXPECT_NE(new_ptr, nullptr);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 6);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator->count(), 2U);

  allocator->Deallocate(new_ptr, new_layout);
  EXPECT_EQ(allocator->used(), sizeof(uint32_t) * 2);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator->count(), 1U);

  allocator->Deallocate(ptr2, old_layout);
  EXPECT_EQ(allocator->used(), 0U);
  EXPECT_EQ(allocator->peak(), sizeof(uint32_t) * 8);
  EXPECT_EQ(allocator->count(), 0U);
}

}  // namespace
}  // namespace pw::allocator
