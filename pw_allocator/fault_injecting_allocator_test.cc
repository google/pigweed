// Copyright 2025 The Pigweed Authors
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

#include "pw_allocator/fault_injecting_allocator.h"

#include "pw_allocator/layout.h"
#include "pw_allocator/testing.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::allocator::Layout;

constexpr size_t kTestBufferSize = 128;

constexpr Layout kSmallLayout = Layout::Of<int>();
constexpr Layout kLargeLayout = Layout::Of<long long>();

TEST(FaultInjectingAllocatorWithBufferTest, AllocateEnableDisable) {
  pw::allocator::test::AllocatorForTest<kTestBufferSize> wrapped_allocator;
  pw::allocator::test::FaultInjectingAllocator allocator(wrapped_allocator);

  allocator.DisableAllocate();

  void* ptr = allocator.Allocate(kSmallLayout);
  EXPECT_EQ(ptr, nullptr);

  allocator.EnableAllocate();
  ptr = allocator.Allocate(kSmallLayout);
  EXPECT_NE(ptr, nullptr);

  allocator.Deallocate(ptr);
}

TEST(FaultInjectingAllocatorWithBufferTest, ResizeEnableDisable) {
  pw::allocator::test::AllocatorForTest<kTestBufferSize> wrapped_allocator;
  pw::allocator::test::FaultInjectingAllocator allocator(wrapped_allocator);

  allocator.EnableAllocate();
  void* ptr =
      allocator.Allocate(kLargeLayout);  // Allocate a larger block initially
  ASSERT_NE(ptr, nullptr);  // Stop test if initial allocation fails

  allocator.DisableResize();
  EXPECT_FALSE(allocator.Resize(ptr, 1));

  allocator.EnableResize();
  EXPECT_TRUE(allocator.Resize(ptr, 1));

  allocator.Deallocate(ptr);
}

TEST(FaultInjectingAllocatorWithBufferTest, ReallocateEnableDisable) {
  pw::allocator::test::AllocatorForTest<kTestBufferSize> wrapped_allocator;
  pw::allocator::test::FaultInjectingAllocator allocator(wrapped_allocator);

  allocator.EnableAllocate();
  void* original_ptr = allocator.Allocate(kSmallLayout);
  ASSERT_NE(original_ptr, nullptr);  // Stop test if initial allocation fails

  allocator.DisableReallocate();

  void* reallocated_ptr = allocator.Reallocate(original_ptr, kLargeLayout);
  EXPECT_EQ(reallocated_ptr, nullptr);

  allocator.EnableReallocate();

  reallocated_ptr = allocator.Reallocate(original_ptr, kLargeLayout);
  EXPECT_NE(reallocated_ptr, nullptr);

  if (reallocated_ptr != nullptr) {
    allocator.Deallocate(reallocated_ptr);
  } else {
    allocator.Deallocate(original_ptr);
  }
}

}  // namespace
