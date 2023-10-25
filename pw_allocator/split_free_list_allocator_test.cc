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
#include "pw_allocator/block.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"

namespace pw::allocator {
namespace {

// Test fixture.

class SplitFreeListAllocatorTest : public ::testing::Test {
 protected:
  static constexpr size_t kCapacity = 256;

  using BlockType = Block<uint16_t, kCapacity>;

  static constexpr size_t kMaxSize = kCapacity - BlockType::kBlockOverhead;
  static constexpr size_t kThreshold = 64;
  static constexpr size_t kNumPtrs = 16;

  void SetUp() override {
    for (size_t i = 0; i < kNumPtrs; ++i) {
      ptrs_[i] = nullptr;
    }
    span_ = ByteSpan(buffer_.data(), buffer_.size());
    ASSERT_EQ(allocator_.Init(span_, kThreshold), OkStatus());
  }

  // This method simply ensures the memory is usable by writing to it.
  void UseMemory(void* ptr, size_t size) { memset(ptr, 0x5a, size); }

  void TearDown() override {
    for (size_t i = 0; i < kNumPtrs; ++i) {
      if (ptrs_[i] != nullptr) {
        // `SplitFreeListAllocator::Deallocate` doesn't actually use the layout,
        // as the information it needs is encoded in the blocks.
        allocator_.Deallocate(ptrs_[i], Layout::Of<void*>());
      }
    }
  }

  alignas(BlockType) std::array<std::byte, kCapacity> buffer_;
  ByteSpan span_;
  SplitFreeListAllocator<BlockType> allocator_;

  // Tests can store allocations in this array to have them automatically
  // freed in `TearDown`, including on ASSERT failure. If pointers are manually
  // deallocated, they should be set to null in the array.
  void* ptrs_[kNumPtrs];
};

// Unit tests.

TEST_F(SplitFreeListAllocatorTest, InitUnaligned) {
  // The test fixture uses aligned memory to make it easier to reason about
  // allocations, but that isn't strictly required.
  SplitFreeListAllocator<Block<>> unaligned;
  EXPECT_EQ(unaligned.Init(span_.subspan(1), kThreshold), OkStatus());
}

TEST_F(SplitFreeListAllocatorTest, AllocateLarge) {
  constexpr Layout layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_.Allocate(layout);
  ASSERT_NE(ptrs_[0], nullptr);
  EXPECT_GE(ptrs_[0], buffer_.data());
  EXPECT_LT(ptrs_[0], buffer_.data() + buffer_.size());
  UseMemory(ptrs_[0], layout.size());
}

TEST_F(SplitFreeListAllocatorTest, AllocateSmall) {
  // Returned pointer should not be from the beginning, but should still be in
  // range. Exact pointer depends on allocator's minimum allocation size.
  constexpr Layout layout = Layout::Of<uint8_t>();
  ptrs_[0] = allocator_.Allocate(layout);
  ASSERT_NE(ptrs_[0], nullptr);
  EXPECT_GT(ptrs_[0], buffer_.data());
  EXPECT_LT(ptrs_[0], buffer_.data() + buffer_.size());
  UseMemory(ptrs_[0], layout.size());
}

TEST_F(SplitFreeListAllocatorTest, AllocateTooLarge) {
  ptrs_[0] = allocator_.Allocate(Layout::Of<std::byte[kCapacity * 2]>());
  EXPECT_EQ(ptrs_[0], nullptr);
}

TEST_F(SplitFreeListAllocatorTest, AllocateLargeAlignment) {
  constexpr size_t kSize = sizeof(uint32_t);
  constexpr size_t kAlignment = 64;
  ptrs_[0] = allocator_.AllocateUnchecked(kSize, kAlignment);
  ASSERT_NE(ptrs_[0], nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptrs_[0]) % kAlignment, 0U);
  UseMemory(ptrs_[0], kSize);

  ptrs_[1] = allocator_.AllocateUnchecked(kSize, kAlignment);
  ASSERT_NE(ptrs_[1], nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptrs_[1]) % kAlignment, 0U);
  UseMemory(ptrs_[0], kSize);
}

TEST_F(SplitFreeListAllocatorTest, AllocateFromUnaligned) {
  SplitFreeListAllocator<Block<>> unaligned;
  ASSERT_EQ(unaligned.Init(span_.subspan(1), kThreshold), OkStatus());

  constexpr Layout layout = Layout::Of<std::byte[kThreshold + 8]>();
  void* ptr = unaligned.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  UseMemory(ptr, layout.size());
  unaligned.Deallocate(ptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, AllocateAlignmentFailure) {
  // Determine the total number of available bytes.
  auto base = reinterpret_cast<uintptr_t>(buffer_.data());
  uintptr_t addr = AlignUp(base, BlockType::kAlignment);
  size_t outer_size = buffer_.size() - (addr - base);

  // The first block is large....
  addr += BlockType::kBlockOverhead + kThreshold;

  // The next block is not aligned...
  constexpr size_t kAlignment = 128;
  uintptr_t next = AlignUp(addr + BlockType::kBlockOverhead, kAlignment / 4);
  if (next % kAlignment == 0) {
    next += kAlignment / 4;
  }

  // And the last block consumes the remaining space.
  // size_t outer_size = allocator_.begin()->OuterSize();
  size_t inner_size1 = next - addr;
  size_t inner_size2 = kThreshold * 2;
  size_t inner_size3 =
      outer_size - (BlockType::kBlockOverhead * 3 + inner_size1 + inner_size2);

  // Allocate all the blocks.
  ptrs_[0] = allocator_.AllocateUnchecked(inner_size1, 1);
  ASSERT_NE(ptrs_[0], nullptr);

  ptrs_[1] = allocator_.AllocateUnchecked(inner_size2, 1);
  ASSERT_NE(ptrs_[1], nullptr);

  ptrs_[2] = allocator_.AllocateUnchecked(inner_size3, 1);
  ASSERT_NE(ptrs_[2], nullptr);

  // If done correctly, the second block's usable space should be unaligned.
  EXPECT_NE(reinterpret_cast<uintptr_t>(ptrs_[1]) % kAlignment, 0U);

  // Free the second region. This leaves an unaligned region available.
  allocator_.DeallocateUnchecked(ptrs_[1], inner_size2, 1);
  ptrs_[1] = nullptr;

  // The allocator should be unable to create an aligned region..
  ptrs_[3] = allocator_.AllocateUnchecked(inner_size2, kAlignment);
  EXPECT_EQ(ptrs_[3], nullptr);
}
TEST_F(SplitFreeListAllocatorTest, DeallocateNull) {
  constexpr Layout layout = Layout::Of<uint8_t>();
  allocator_.Deallocate(nullptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, DeallocateShuffled) {
  constexpr Layout layout = Layout::Of<std::byte[32]>();
  // Allocate until the pool is exhausted.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    ptrs_[i] = allocator_.Allocate(layout);
    if (ptrs_[i] == nullptr) {
      break;
    }
  }
  // Mix up the order of allocations.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    if (i % 2 == 0 && i + 1 < kNumPtrs) {
      std::swap(ptrs_[i], ptrs_[i + 1]);
    }
    if (i % 3 == 0 && i + 2 < kNumPtrs) {
      std::swap(ptrs_[i], ptrs_[i + 2]);
    }
  }
  // Deallocate everything.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    allocator_.Deallocate(ptrs_[i], layout);
    ptrs_[i] = nullptr;
  }
}

TEST_F(SplitFreeListAllocatorTest, QueryLargeValid) {
  constexpr Layout layout = Layout::Of<std::byte[kThreshold * 2]>();
  ptrs_[0] = allocator_.Allocate(layout);
  EXPECT_EQ(allocator_.Query(ptrs_[0], layout), OkStatus());
}

TEST_F(SplitFreeListAllocatorTest, QuerySmallValid) {
  constexpr Layout layout = Layout::Of<uint8_t>();
  ptrs_[0] = allocator_.Allocate(layout);
  EXPECT_EQ(allocator_.Query(ptrs_[0], layout), OkStatus());
}

TEST_F(SplitFreeListAllocatorTest, QueryInvalidPtr) {
  constexpr Layout layout = Layout::Of<SplitFreeListAllocatorTest>();
  EXPECT_EQ(allocator_.Query(this, layout), Status::OutOfRange());
}

TEST_F(SplitFreeListAllocatorTest, ResizeNull) {
  constexpr Layout old_layout = Layout::Of<uint8_t>();
  size_t new_size = 1;
  EXPECT_FALSE(allocator_.Resize(nullptr, old_layout, new_size));
}

TEST_F(SplitFreeListAllocatorTest, ResizeSame) {
  constexpr Layout old_layout = Layout::Of<uint32_t>();
  ptrs_[0] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(allocator_.Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeSmaller) {
  constexpr Layout old_layout = Layout::Of<std::byte[kMaxSize]>();
  ptrs_[0] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Shrinking always succeeds.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold]>();
  EXPECT_TRUE(allocator_.Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeLarger) {
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Nothing after ptr, so `Resize` should succeed.
  constexpr Layout new_layout = Layout::Of<std::byte[kMaxSize]>();
  EXPECT_TRUE(allocator_.Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeLargerFailure) {
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  ptrs_[1] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[1], nullptr);

  // Memory after ptr is already allocated, so `Resize` should fail.
  EXPECT_FALSE(allocator_.Resize(ptrs_[0], old_layout, kMaxSize));
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeSmallerAcrossThreshold) {
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Shrinking succeeds, and the pointer is unchanged even though it is now
  // below the threshold.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold / 4]>();
  EXPECT_TRUE(allocator_.Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallSmaller) {
  constexpr Layout old_layout = Layout::Of<uint32_t>();
  ptrs_[0] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Shrinking always succeeds.
  constexpr Layout new_layout = Layout::Of<uint8_t>();
  EXPECT_TRUE(allocator_.Resize(ptrs_[0], old_layout, new_layout.size()));
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLarger) {
  // First, allocate a trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[0] = allocator_.Allocate(layout1);
  ASSERT_NE(ptrs_[0], nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[1] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[1], nullptr);

  // Now free the trailing block.
  allocator_.Deallocate(ptrs_[0], layout1);
  ptrs_[0] = nullptr;

  // And finally, resize. Since the memory after the block is available and big
  // enough, `Resize` should succeed.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold / 2]>();
  EXPECT_TRUE(allocator_.Resize(ptrs_[1], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[1], nullptr);
  UseMemory(ptrs_[1], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLargerFailure) {
  // First, allocate a trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[0] = allocator_.Allocate(layout1);
  ASSERT_NE(ptrs_[0], nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[1] = allocator_.Allocate(old_layout);
  ASSERT_NE(ptrs_[1], nullptr);

  // Now free the trailing block.
  allocator_.Deallocate(ptrs_[0], layout1);
  ptrs_[0] = nullptr;

  // And finally, resize. Since the memory after the block is available but not
  // big enough, `Resize` should fail.
  size_t new_size = 48;
  EXPECT_FALSE(allocator_.Resize(ptrs_[1], old_layout, new_size));
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLargerAcrossThreshold) {
  // First, allocate several trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[kThreshold / 2]>();
  ptrs_[0] = allocator_.Allocate(layout1);
  ASSERT_NE(ptrs_[0], nullptr);

  ptrs_[1] = allocator_.Allocate(layout1);
  ASSERT_NE(ptrs_[1], nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[2] = allocator_.Allocate(old_layout);
  EXPECT_NE(ptrs_[2], nullptr);

  // Now free the trailing blocks.
  allocator_.Deallocate(ptrs_[0], layout1);
  ptrs_[0] = nullptr;
  allocator_.Deallocate(ptrs_[1], layout1);
  ptrs_[1] = nullptr;

  // Growing succeeds, and the pointer is unchanged even though it is now
  // above the threshold.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold]>();
  EXPECT_TRUE(allocator_.Resize(ptrs_[2], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[2], nullptr);
  UseMemory(ptrs_[2], new_layout.size());
}

}  // namespace
}  // namespace pw::allocator
