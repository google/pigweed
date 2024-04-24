// Copyright 2024 The Pigweed Authors
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

#include "pw_allocator/block_allocator_testing.h"

#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"
#include "pw_status/status.h"

namespace pw::allocator::test {

// Test fixtures.

BlockAllocatorTest::BlockAllocatorTest(BlockAllocatorType& allocator)
    : ::testing::Test(), allocator_(allocator) {}

void BlockAllocatorTest::SetUp() { ptrs_.fill(nullptr); }

ByteSpan BlockAllocatorTest::GetBytes() {
  return ByteSpan(buffer_.data(), buffer_.size());
}

BlockAllocatorTest::BlockAllocatorType& BlockAllocatorTest::GetAllocator() {
  EXPECT_EQ(allocator_.Init(GetBytes()), OkStatus());
  return allocator_;
}

BlockAllocatorTest::BlockAllocatorType& BlockAllocatorTest::GetAllocator(
    std::initializer_list<Preallocation> preallocations) {
  // First, look if any blocks use kSizeRemaining, and calculate how large
  // that will be.
  size_t remaining_outer_size = kCapacity;
  for (auto& preallocation : preallocations) {
    if (preallocation.outer_size != Preallocation::kSizeRemaining) {
      size_t outer_size =
          AlignUp(preallocation.outer_size, BlockType::kAlignment);
      PW_CHECK_INT_GE(remaining_outer_size, outer_size);
      remaining_outer_size -= outer_size;
    }
  }

  Result<BlockType*> result = BlockType::Init(GetBytes());
  PW_CHECK_OK(result.status());
  BlockType* block = *result;
  void* begin = block->UsableSpace();

  // To prevent free blocks being merged back into the block of available
  // space, treat the available space as being used.
  block->MarkUsed();

  size_t next_index = 0;
  for (auto& preallocation : preallocations) {
    PW_CHECK_NOTNULL(block);

    // Perform the allocation.
    size_t outer_size = preallocation.outer_size;
    if (outer_size == Preallocation::kSizeRemaining) {
      outer_size = remaining_outer_size;
      remaining_outer_size = 0;
    }
    size_t inner_size = outer_size - BlockType::kBlockOverhead;

    block->MarkFree();
    PW_CHECK_OK(BlockType::AllocFirst(block, inner_size, 1));
    if (!block->Last()) {
      block->Next()->MarkUsed();
    }

    // Free the block or cache the allocated pointer.
    if (preallocation.index == Preallocation::kIndexFree) {
      BlockType::Free(block);

    } else if (preallocation.index == Preallocation::kIndexNext) {
      while (true) {
        PW_CHECK_INT_LT(next_index, kNumPtrs);
        if (Fetch(next_index) == nullptr &&
            std::all_of(preallocations.begin(),
                        preallocations.end(),
                        [next_index](const Preallocation& other) {
                          return other.index != next_index;
                        })) {
          break;
        }
        ++next_index;
      }
      Store(next_index, block->UsableSpace());

    } else {
      Store(preallocation.index, block->UsableSpace());
    }
    block = block->Next();
  }
  if (block != nullptr) {
    block->MarkFree();
  }
  block = BlockType::FromUsableSpace(begin);
  PW_CHECK_OK(allocator_.Init(block));
  return allocator_;
}

void* BlockAllocatorTest::NextAfter(size_t index) {
  if (index > ptrs_.size()) {
    return nullptr;
  }
  auto* block = BlockType::FromUsableSpace(Fetch(index));
  while (!block->Last()) {
    block = block->Next();
    if (block->Used()) {
      return block->UsableSpace();
    }
  }
  return nullptr;
}

void BlockAllocatorTest::Store(size_t index, void* ptr) { ptrs_[index] = ptr; }

void* BlockAllocatorTest::Fetch(size_t index) { return ptrs_[index]; }

void BlockAllocatorTest::UseMemory(void* ptr, size_t size) {
  std::memset(ptr, 0x5a, size);
}

void BlockAllocatorTest::TearDown() {
  for (size_t i = 0; i < kNumPtrs; ++i) {
    if (Fetch(i) != nullptr) {
      allocator_.Deallocate(Fetch(i));
    }
  }
  allocator_.Reset();
}

// Unit tests.

void BlockAllocatorTest::CanAutomaticallyInit(BlockAllocatorType& allocator) {
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

void BlockAllocatorTest::CanExplicitlyInit(BlockAllocatorType& allocator) {
  EXPECT_EQ(*(allocator.blocks().begin()), nullptr);
  EXPECT_EQ(allocator.Init(GetBytes()), OkStatus());
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

void BlockAllocatorTest::GetCapacity() {
  auto& allocator = GetAllocator();
  StatusWithSize capacity = allocator.GetCapacity();
  EXPECT_EQ(capacity.status(), OkStatus());
  EXPECT_EQ(capacity.size(), kCapacity);
}

void BlockAllocatorTest::AllocateLarge() {
  auto& allocator = GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kLargeInnerSize]>();
  Store(0, allocator.Allocate(layout));
  ASSERT_NE(Fetch(0), nullptr);
  ByteSpan bytes = GetBytes();
  EXPECT_GE(Fetch(0), bytes.data());
  EXPECT_LE(Fetch(0), bytes.data() + bytes.size());
  UseMemory(Fetch(0), layout.size());
}

void BlockAllocatorTest::AllocateSmall() {
  auto& allocator = GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();
  Store(0, allocator.Allocate(layout));
  ASSERT_NE(Fetch(0), nullptr);
  ByteSpan bytes = GetBytes();
  EXPECT_GE(Fetch(0), bytes.data());
  EXPECT_LE(Fetch(0), bytes.data() + bytes.size());
  UseMemory(Fetch(0), layout.size());
}

void BlockAllocatorTest::AllocateTooLarge() {
  auto& allocator = GetAllocator();
  Store(0, allocator.Allocate(Layout::Of<std::byte[kCapacity * 2]>()));
  EXPECT_EQ(Fetch(0), nullptr);
}

void BlockAllocatorTest::AllocateLargeAlignment() {
  auto& allocator = GetAllocator();
  constexpr size_t kAlignment = 64;
  Store(0, allocator.Allocate(Layout(kLargeInnerSize, kAlignment)));
  ASSERT_NE(Fetch(0), nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(Fetch(0)) % kAlignment, 0U);
  UseMemory(Fetch(0), kLargeInnerSize);

  Store(1, allocator.Allocate(Layout(kLargeInnerSize, kAlignment)));
  ASSERT_NE(Fetch(1), nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(Fetch(1)) % kAlignment, 0U);
  UseMemory(Fetch(1), kLargeInnerSize);
}

void BlockAllocatorTest::AllocateAlignmentFailure() {
  // Allocate a two blocks with an unaligned region between them.
  constexpr size_t kAlignment = 128;
  ByteSpan bytes = GetBytes();
  auto addr = reinterpret_cast<uintptr_t>(bytes.data());
  uintptr_t outer_size =
      AlignUp(addr + BlockType::kBlockOverhead, kAlignment) - addr + 1;
  auto& allocator = GetAllocator({
      {outer_size, 0},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 2},
  });

  // The allocator should be unable to create an aligned region..
  Store(1, allocator.Allocate(Layout(kLargeInnerSize, kAlignment)));
  EXPECT_EQ(Fetch(1), nullptr);
}

void BlockAllocatorTest::DeallocateNull() {
  auto& allocator = GetAllocator();
  allocator.Deallocate(nullptr);
}

void BlockAllocatorTest::DeallocateShuffled() {
  auto& allocator = GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();
  for (size_t i = 0; i < kNumPtrs; ++i) {
    Store(i, allocator.Allocate(layout));
    if (Fetch(i) == nullptr) {
      break;
    }
  }

  // Mix up the order of allocations.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    if (i % 2 == 0 && i + 1 < kNumPtrs) {
      void* tmp = Fetch(i);
      Store(i, Fetch(i + 1));
      Store(i + 1, tmp);
    }
    if (i % 3 == 0 && i + 2 < kNumPtrs) {
      void* tmp = Fetch(i);
      Store(i, Fetch(i + 2));
      Store(i + 2, tmp);
    }
  }

  // Deallocate everything.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    allocator.Deallocate(Fetch(i));
    Store(i, nullptr);
  }
}

void BlockAllocatorTest::IterateOverBlocks() {
  auto& allocator = GetAllocator({
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kLargeOuterSize, Preallocation::kIndexNext},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kLargeOuterSize, Preallocation::kIndexNext},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kLargeOuterSize, Preallocation::kIndexNext},
      {Preallocation::kSizeRemaining, Preallocation::kIndexFree},
  });

  // Count the blocks. The unallocated ones vary in size, but the allocated ones
  // should all be the same.
  size_t free_count = 0;
  size_t used_count = 0;
  for (auto* block : allocator.blocks()) {
    if (block->Used()) {
      EXPECT_EQ(block->InnerSize(), kLargeInnerSize);
      ++used_count;
    } else {
      ++free_count;
    }
  }
  EXPECT_EQ(used_count, 3U);
  EXPECT_EQ(free_count, 4U);
}

void BlockAllocatorTest::ResizeNull() {
  auto& allocator = GetAllocator();
  size_t new_size = 1;
  EXPECT_FALSE(allocator.Resize(nullptr, new_size));
}

void BlockAllocatorTest::ResizeLargeSame() {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, 0},
      {kLargeOuterSize, 1},
  });
  size_t new_size = kLargeInnerSize;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kLargeInnerSize);
}

void BlockAllocatorTest::ResizeLargeSmaller() {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, 0},
      {kLargeOuterSize, 1},
  });
  size_t new_size = kSmallInnerSize;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize);
}

void BlockAllocatorTest::ResizeLargeLarger() {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, 0},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallOuterSize, 2},
  });
  size_t new_size = kLargeInnerSize * 2;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kLargeInnerSize * 2);
}

void BlockAllocatorTest::ResizeLargeLargerFailure() {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, 0},
      {kSmallOuterSize, 12},
  });
  // Memory after ptr is already allocated, so `Resize` should fail.
  size_t new_size = kLargeInnerSize * 2;
  EXPECT_FALSE(allocator.Resize(Fetch(0), new_size));
}

void BlockAllocatorTest::ResizeSmallSame() {
  auto& allocator = GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, 1},
  });
  size_t new_size = kSmallInnerSize;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize);
}

void BlockAllocatorTest::ResizeSmallSmaller() {
  auto& allocator = GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, 1},
  });
  size_t new_size = kSmallInnerSize / 2;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize / 2);
}

void BlockAllocatorTest::ResizeSmallLarger() {
  auto& allocator = GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallOuterSize, 2},
  });
  size_t new_size = kSmallInnerSize * 2;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize * 2);
}

void BlockAllocatorTest::ResizeSmallLargerFailure() {
  auto& allocator = GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, 1},
  });
  // Memory after ptr is already allocated, so `Resize` should fail.
  size_t new_size = kSmallInnerSize * 2 + BlockType::kBlockOverhead;
  EXPECT_FALSE(allocator.Resize(Fetch(0), new_size));
}

void BlockAllocatorTest::CanGetLayoutFromValidPointer() {
  auto& allocator = GetAllocator();
  constexpr size_t kAlignment = 64;
  Store(0, allocator.Allocate(Layout(kLargeInnerSize, kAlignment * 2)));
  ASSERT_NE(Fetch(0), nullptr);

  Store(1, allocator.Allocate(Layout(kSmallInnerSize, kAlignment / 2)));
  ASSERT_NE(Fetch(1), nullptr);

  Result<Layout> result0 = allocator.GetLayout(Fetch(0));
  ASSERT_EQ(result0.status(), OkStatus());
  EXPECT_GE(result0->size(), kLargeInnerSize);
  EXPECT_EQ(result0->alignment(), kAlignment * 2);

  Result<Layout> result1 = allocator.GetLayout(Fetch(1));
  ASSERT_EQ(result1.status(), OkStatus());
  EXPECT_GE(result1->size(), kSmallInnerSize);
  EXPECT_EQ(result1->alignment(), kAlignment / 2);
}

void BlockAllocatorTest::CannotGetLayoutFromInvalidPointer() {
  auto& allocator = GetAllocator({
      {kLargerOuterSize, 0},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallOuterSize, 2},
      {kSmallerOuterSize, Preallocation::kIndexFree},
      {kSmallOuterSize, 4},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kLargerOuterSize, 6},
  });

  Result<Layout> result0 = allocator.GetLayout(nullptr);
  EXPECT_EQ(result0.status(), Status::NotFound());

  for (const auto& block : allocator.blocks()) {
    if (!block->Used()) {
      Result<Layout> result1 = allocator.GetLayout(block->UsableSpace());
      EXPECT_EQ(result1.status(), Status::FailedPrecondition());
    }
  }
}

}  // namespace pw::allocator::test
