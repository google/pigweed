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

#include "pw_allocator/split_free_list_allocator.h"

#include "gtest/gtest.h"
#include "pw_bytes/alignment.h"
#include "pw_containers/vector.h"

namespace pw::allocator {
namespace {

// Test fixture.

struct SplitFreeListAllocatorTest : ::testing::Test {
  alignas(16) std::array<std::byte, 256> buffer;
  SplitFreeListAllocator allocator;

  void SetUp() override {
    allocator.Initialize(buffer.data(), buffer.size(), 64);
  }
};

// Unit tests.

TEST_F(SplitFreeListAllocatorTest, InitializeUnaligned) {
  // The test fixture uses aligned memory to make it easier to reason about
  // allocations, but that isn't strictly required. Simply verify that a call to
  // `Initialize` with unaligned memory does not crash.
  alignas(16) std::array<std::byte, 256> buf;
  SplitFreeListAllocator unaligned;
  unaligned.Initialize(buf.data() + 1, buf.size() - 1, 64);
}

TEST_F(SplitFreeListAllocatorTest, AllocateLargeDeallocate) {
  constexpr Layout layout = Layout::Of<std::byte[64]>();
  void* ptr = allocator.Allocate(layout);
  // Returned pointer should be from the beginning.
  EXPECT_EQ(ptr, buffer.data());
  allocator.Deallocate(ptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, AllocateSmallDeallocate) {
  // Returned pointer should not be from the beginning, but should still be in
  // range. Exact pointer depends on allocator's minimum allocation size.
  constexpr Layout layout = Layout::Of<uint8_t>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_GT(ptr, buffer.data());
  EXPECT_LT(ptr, buffer.data() + buffer.size());
  allocator.Deallocate(ptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, AllocateTooLarge) {
  void* ptr = allocator.Allocate(Layout::Of<std::byte[512]>());
  EXPECT_EQ(ptr, nullptr);
}

TEST_F(SplitFreeListAllocatorTest, AllocateAllDeallocateShuffled) {
  constexpr Layout layout = Layout::Of<std::byte[32]>();
  Vector<void*, 256> ptrs;
  // Allocate until the pool is exhausted.
  while (true) {
    void* ptr = allocator.Allocate(layout);
    if (ptr == nullptr) {
      break;
    }
    ptrs.push_back(ptr);
  }
  // Mix up the order of allocations.
  for (size_t i = 0; i < ptrs.size(); ++i) {
    if (i % 2 == 0 && i + 1 < ptrs.size()) {
      std::swap(ptrs[i], ptrs[i + 1]);
    }
    if (i % 3 == 0 && i + 2 < ptrs.size()) {
      std::swap(ptrs[i], ptrs[i + 2]);
    }
  }
  // Deallocate everything.
  for (void* ptr : ptrs) {
    allocator.Deallocate(ptr, layout);
  }
}

TEST_F(SplitFreeListAllocatorTest, AllocateDeallocateLargeAlignment) {
  void* ptr1 = allocator.AllocateUnchecked(sizeof(uint32_t), 64);
  void* ptr2 = allocator.AllocateUnchecked(sizeof(uint32_t), 64);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr1) % 64, 0U);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr2) % 64, 0U);
  allocator.DeallocateUnchecked(ptr1, sizeof(uint32_t), 64);
  allocator.DeallocateUnchecked(ptr2, sizeof(uint32_t), 64);
}

TEST_F(SplitFreeListAllocatorTest, AllocateAlignmentFailure) {
  // Find a valid address aligned to 128 bytes.
  auto base = reinterpret_cast<uintptr_t>(buffer.data());
  auto aligned = AlignUp(base + 16, 128);

  // Now allocate up to 3 regions:
  //   * from the beginning to 16 bytes before the alignment boundary
  //   * the next 128 bytes
  //   * whatever is left
  size_t size1 = aligned - base - 16;
  void* ptr1 = allocator.AllocateUnchecked(size1, 1);

  size_t size2 = 128;
  void* ptr2 = allocator.AllocateUnchecked(size2, 1);

  size_t size3 = 128 - size1;
  void* ptr3 = allocator.AllocateUnchecked(size3, 1);

  // Now free the second region. This leaves a 128-byte region available, but it
  // is not aligned to a 128 byte boundary.
  allocator.DeallocateUnchecked(ptr2, size2, 1);

  // The allocator should be unable to create an aligned region of the given
  // size.
  void* ptr = allocator.AllocateUnchecked(128, 128);
  EXPECT_EQ(ptr, nullptr);

  if (ptr1 != nullptr) {
    allocator.DeallocateUnchecked(ptr1, size1, 1);
  }
  allocator.DeallocateUnchecked(ptr3, size3, 1);
}

TEST_F(SplitFreeListAllocatorTest, DeallocateNull) {
  constexpr Layout layout = Layout::Of<uint8_t>();
  allocator.Deallocate(nullptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, QueryLargeValid) {
  constexpr Layout layout = Layout::Of<std::byte[128]>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_EQ(allocator.Query(ptr, layout), OkStatus());
  allocator.Deallocate(ptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, QuerySmallValid) {
  constexpr Layout layout = Layout::Of<uint8_t>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_EQ(allocator.Query(ptr, layout), OkStatus());
  allocator.Deallocate(ptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, QueryInvalidPtr) {
  constexpr Layout layout = Layout::Of<SplitFreeListAllocatorTest>();
  EXPECT_EQ(allocator.Query(this, layout), Status::OutOfRange());
}

TEST_F(SplitFreeListAllocatorTest, QueryInvalidSize) {
  constexpr Layout layout = Layout::Of<uint8_t>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_EQ(allocator.QueryUnchecked(ptr, 0, layout.alignment()),
            Status::OutOfRange());
  allocator.Deallocate(ptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeNull) {
  constexpr Layout old_layout = Layout::Of<uint8_t>();
  size_t new_size = 1;
  EXPECT_FALSE(allocator.Resize(nullptr, old_layout, new_size));
}

TEST_F(SplitFreeListAllocatorTest, ResizeSame) {
  constexpr Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);
  EXPECT_NE(ptr, nullptr);
  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, new_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeSmaller) {
  constexpr Layout old_layout = Layout::Of<std::byte[240]>();
  void* ptr = allocator.Allocate(old_layout);

  // Shrinking always succeeds.
  constexpr Layout new_layout = Layout::Of<std::byte[80]>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, new_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeLarger) {
  constexpr Layout old_layout = Layout::Of<std::byte[80]>();
  void* ptr = allocator.Allocate(old_layout);

  // Nothing after ptr, so `Resize` should succeed.
  constexpr Layout new_layout = Layout::Of<std::byte[240]>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, new_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeLargerFailure) {
  constexpr Layout old_layout = Layout::Of<std::byte[80]>();
  void* ptr1 = allocator.Allocate(old_layout);
  void* ptr2 = allocator.Allocate(old_layout);

  // Memory after ptr is already allocated, so `Resize` should fail.
  size_t new_size = 240;
  EXPECT_FALSE(allocator.Resize(ptr1, old_layout, new_size));
  allocator.Deallocate(ptr1, old_layout);
  allocator.Deallocate(ptr2, old_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeSmallerAcrossThreshold) {
  constexpr Layout old_layout = Layout::Of<std::byte[80]>();
  void* ptr = allocator.Allocate(old_layout);

  // Shrinking succeeds, and the pointer is unchanged even though it is now
  // below the threshold.
  constexpr Layout new_layout = Layout::Of<std::byte[16]>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, new_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallSmaller) {
  constexpr Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);

  // Shrinking always succeeds.
  constexpr Layout new_layout = Layout::Of<uint8_t>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, new_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLarger) {
  // First, allocate a trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[16]>();
  void* ptr1 = allocator.Allocate(layout1);
  EXPECT_NE(ptr1, nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[16]>();
  void* ptr = allocator.Allocate(old_layout);
  EXPECT_NE(ptr, nullptr);

  // Now free the trailing block.
  allocator.Deallocate(ptr1, layout1);

  // And finally, resize. Since the memory after the block is available and big
  // enough, `Resize` should succeed.
  constexpr Layout new_layout = Layout::Of<std::byte[24]>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, new_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLargerFailure) {
  // First, allocate a trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[8]>();
  void* ptr1 = allocator.Allocate(layout1);
  EXPECT_NE(ptr1, nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[16]>();
  void* ptr = allocator.Allocate(old_layout);
  EXPECT_NE(ptr, nullptr);

  // Now free the trailing block.
  allocator.Deallocate(ptr1, layout1);

  // And finally, resize. Since the memory after the block is available but not
  // big enough, `Resize` should fail.
  size_t new_size = 48;
  EXPECT_FALSE(allocator.Resize(ptr, old_layout, new_size));
  allocator.Deallocate(ptr, old_layout);
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLargerAcrossThreshold) {
  // First, allocate several trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[48]>();
  void* ptr1 = allocator.Allocate(layout1);
  EXPECT_NE(ptr1, nullptr);
  void* ptr2 = allocator.Allocate(layout1);
  EXPECT_NE(ptr2, nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[16]>();
  void* ptr = allocator.Allocate(old_layout);
  EXPECT_NE(ptr, nullptr);

  // Now free the trailing blocks.
  allocator.Deallocate(ptr1, layout1);
  allocator.Deallocate(ptr2, layout1);

  // Growing succeeds, and the pointer is unchanged even though it is now
  // above the threshold.
  constexpr Layout new_layout = Layout::Of<std::byte[96]>();
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_layout.size()));
  allocator.Deallocate(ptr, new_layout);
}

}  // namespace
}  // namespace pw::allocator
