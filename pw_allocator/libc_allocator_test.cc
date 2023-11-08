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

#include "pw_allocator/libc_allocator.h"

#include <cstring>

#include "gtest/gtest.h"

namespace pw::allocator {

// Test fixtures.

class LibCAllocatorTest : public ::testing::Test {
 protected:
  LibCAllocator allocator;
};

// Unit tests.

TEST_F(LibCAllocatorTest, AllocateDeallocate) {
  constexpr Layout layout = Layout::Of<std::byte[64]>();
  void* ptr = allocator.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  // Check that the pointer can be dereferenced.
  memset(ptr, 0xAB, layout.size());
  allocator.Deallocate(ptr, layout);
}

TEST_F(LibCAllocatorTest, AllocateLargeAlignment) {
  /// TODO: b/301930507 - `aligned_alloc` is not portable. As a result, this
  /// allocator has a maximum alignment of `std::align_max_t`.
  size_t size = 16;
  size_t alignment = alignof(std::max_align_t) * 2;
  void* ptr = allocator.AllocateUnchecked(size, alignment);
  EXPECT_EQ(ptr, nullptr);
}

TEST_F(LibCAllocatorTest, Resize) {
  constexpr Layout old_layout = Layout::Of<uint32_t[4]>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  constexpr Layout new_layout = Layout::Of<uint32_t[3]>();
  EXPECT_FALSE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, old_layout);
}

TEST_F(LibCAllocatorTest, ResizeSame) {
  constexpr Layout layout = Layout::Of<uint32_t[4]>();
  void* ptr = allocator.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  EXPECT_TRUE(allocator.Resize(ptr, layout, layout.size()));
  allocator.Deallocate(ptr, layout);
}

TEST_F(LibCAllocatorTest, Reallocate) {
  constexpr Layout old_layout = Layout::Of<uint32_t[4]>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  constexpr Layout new_layout = Layout::Of<uint32_t[3]>();
  void* new_ptr = allocator.Reallocate(ptr, old_layout, new_layout.size());
  ASSERT_NE(new_ptr, nullptr);
  allocator.Deallocate(new_ptr, new_layout);
}

}  // namespace pw::allocator
