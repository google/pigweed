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
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include "pw_allocator/block.h"
#include "pw_allocator/block_allocator.h"
#include "pw_assert/assert.h"
#include "pw_bytes/alignment.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator::test {

// Forward declaration.
struct Preallocation;

/// Test fixture responsible for managing a memory region and an allocator that
/// allocates block of memory from it.
///
/// This base class contains all the code that does not depend specific
/// `Block` or `BlockAllocator` types.
class BlockAllocatorTestBase : public ::testing::Test {
 public:
  static constexpr size_t kDefaultBlockOverhead = Block<>::kBlockOverhead;

  // Size of the memory region to use in the tests below.
  static constexpr size_t kCapacity = 1024;

  // The number of allocated pointers cached by the test fixture.
  static constexpr size_t kNumPtrs = 16;

  // Represents the sizes of various allocations.
  static constexpr size_t kLargeInnerSize = kCapacity / 8;
  static constexpr size_t kLargeOuterSize =
      kDefaultBlockOverhead + kLargeInnerSize;

  static constexpr size_t kSmallInnerSize = kDefaultBlockOverhead * 2;
  static constexpr size_t kSmallOuterSize =
      kDefaultBlockOverhead + kSmallInnerSize;

  static constexpr size_t kSmallerOuterSize = kSmallInnerSize;
  static constexpr size_t kLargerOuterSize =
      kLargeOuterSize + kSmallerOuterSize;

 protected:
  // Test fixtures.
  void SetUp() override;

  /// Returns the underlying memory region.
  virtual ByteSpan GetBytes() = 0;

  /// Initialize the allocator with a region of memory and return it.
  virtual Allocator& GetAllocator() = 0;

  /// Initialize the allocator with a sequence of preallocated blocks and return
  /// it.
  ///
  /// See also ``Preallocation``.
  virtual Allocator& GetAllocator(
      std::initializer_list<Preallocation> preallocations) = 0;

  /// Gets the next allocation from an allocated pointer.
  virtual void* NextAfter(size_t index) = 0;

  /// Store an allocated pointer in the test's cache of pointers.
  void Store(size_t index, void* ptr);

  /// Retrieve an allocated pointer from the test's cache of pointers.
  void* Fetch(size_t index);

  /// Ensures the memory is usable by writing to it.
  void UseMemory(void* ptr, size_t size);

  // Unit tests.
  void GetCapacity();
  void AllocateLarge();
  void AllocateSmall();
  void AllocateTooLarge();
  void AllocateLargeAlignment();
  void AllocateAlignmentFailure();
  void DeallocateNull();
  void DeallocateShuffled();
  void ResizeNull();
  void ResizeLargeSame();
  void ResizeLargeSmaller();
  void ResizeLargeLarger();
  void ResizeLargeLargerFailure();
  void ResizeSmallSame();
  void ResizeSmallSmaller();
  void ResizeSmallLarger();
  void ResizeSmallLargerFailure();
  void CanGetLayoutFromValidPointer();

 private:
  std::array<void*, kNumPtrs> ptrs_;
};

/// Test fixture responsible for managing a memory region and an allocator that
/// allocates block of memory from it.
///
/// This derived class contains all the code that depends specific `Block` or
/// `BlockAllocator` types.
///
/// @tparam BlockAllocatorType  The type of the `BlockAllocator` being tested.
template <typename BlockAllocatorType>
class BlockAllocatorTest : public BlockAllocatorTestBase {
 public:
  using BlockType = typename BlockAllocatorType::BlockType;

 protected:
  // Test fixtures.
  BlockAllocatorTest(BlockAllocatorType& allocator) : allocator_(allocator) {}

  ByteSpan GetBytes() override { return ByteSpan(buffer_); }

  Allocator& GetAllocator() override;

  Allocator& GetAllocator(
      std::initializer_list<Preallocation> preallocations) override;

  void* NextAfter(size_t index) override;

  void TearDown() override;

  // Unit tests.
  static void CanAutomaticallyInit(BlockAllocatorType& allocator);
  void CanExplicitlyInit(BlockAllocatorType& allocator);
  void IterateOverBlocks();
  void CannotGetLayoutFromInvalidPointer();

 private:
  BlockAllocatorType& allocator_;
  alignas(BlockType::kAlignment) std::array<std::byte, kCapacity> buffer_;
};

/// Represents an initial state for a memory block.
///
/// Unit tests can specify an initial block layout by passing a list of these
/// structs to `Preallocate`.
///
/// The outer size of each block must be at least `kBlockOverhead` for the
/// block type in use. The special `kSizeRemaining` may be used for at most
/// one block to give it any space not assigned to other blocks.
///
/// The index must be less than `BlockAllocatorBlockAllocatorTest::kNumPtrs` or
/// one of the special values `kIndexFree` or `kIndexNext`. A regular index will
/// mark the block as "used" and cache the pointer to its usable space in
/// `ptrs[index]`. The special value `kIndexFree` will leave the block as
/// "free". The special value `kIndexNext` will mark the block as "used" and
/// cache its pointer in the next available slot in `test_fixture`. This
/// may be used/ when the pointer is not needed for the test but should still
/// be automatically freed at the end of the test.
///
/// Example:
/// @code{.cpp}
///   // BlockType = UnpoisonedBlock<uint32_t>, so kBlockOverhead == 8.
///   ASSERT_EQ(Preallocate({
///     {32,              0},           // ptrs[0] == 24 byte region.
///     {24,              kIndexFree},  // Free block of 16 bytes.
///     {48,              2},           // ptrs[2] == 40 byte region.
///     {kSizeRemaining,  kIndesFree},  // Free block of leftover space.
///     {64,              4},           // ptrs[4] == 56 byte region from the
///                                     //             end of the allocator.
///   }), OkStatus());
/// @endcode
struct Preallocation {
  /// The outer size of the block to preallocate.
  size_t outer_size;

  // Index into the `test_fixture` array where the pointer to the block's
  // space should be cached.
  size_t index;

  /// Special value indicating the block should comprise of the all remaining
  /// space not preallocated to any other block. May be used at most once.
  static constexpr size_t kSizeRemaining = std::numeric_limits<size_t>::max();

  /// Special value indicating the block should be treated as unallocated,
  /// i.e. it's pointer should not be cached.
  static constexpr size_t kIndexFree = BlockAllocatorTestBase::kNumPtrs + 1;

  /// Special value indicating to use the next available index.
  static constexpr size_t kIndexNext = BlockAllocatorTestBase::kNumPtrs + 2;
};

// Test fixture template method implementations.

template <typename BlockAllocatorType>
Allocator& BlockAllocatorTest<BlockAllocatorType>::GetAllocator() {
  EXPECT_EQ(allocator_.Init(GetBytes()), OkStatus());
  return allocator_;
}

template <typename BlockAllocatorType>
Allocator& BlockAllocatorTest<BlockAllocatorType>::GetAllocator(
    std::initializer_list<Preallocation> preallocations) {
  // First, look if any blocks use kSizeRemaining, and calculate how large
  // that will be.
  size_t remaining_outer_size = kCapacity;
  for (auto& preallocation : preallocations) {
    if (preallocation.outer_size != Preallocation::kSizeRemaining) {
      size_t outer_size =
          AlignUp(preallocation.outer_size, BlockType::kAlignment);
      PW_ASSERT(remaining_outer_size >= outer_size);
      remaining_outer_size -= outer_size;
    }
  }

  Result<BlockType*> result = BlockType::Init(GetBytes());
  PW_ASSERT(result.ok());
  BlockType* block = *result;
  void* begin = block->UsableSpace();

  // To prevent free blocks being merged back into the block of available
  // space, treat the available space as being used.
  block->MarkUsed();

  size_t next_index = 0;
  for (auto& preallocation : preallocations) {
    PW_ASSERT(block != nullptr);

    // Perform the allocation.
    size_t outer_size = preallocation.outer_size;
    if (outer_size == Preallocation::kSizeRemaining) {
      outer_size = remaining_outer_size;
      remaining_outer_size = 0;
    }
    size_t inner_size = outer_size - BlockType::kBlockOverhead;

    block->MarkFree();
    PW_ASSERT(BlockType::AllocFirst(block, inner_size, 1).ok());
    if (!block->Last()) {
      block->Next()->MarkUsed();
    }

    // Free the block or cache the allocated pointer.
    if (preallocation.index == Preallocation::kIndexFree) {
      BlockType::Free(block);

    } else if (preallocation.index == Preallocation::kIndexNext) {
      while (true) {
        PW_ASSERT(next_index < kNumPtrs);
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
  PW_ASSERT(allocator_.Init(block).ok());
  return allocator_;
}

template <typename BlockAllocatorType>
void* BlockAllocatorTest<BlockAllocatorType>::NextAfter(size_t index) {
  void* ptr = Fetch(index);
  if (ptr == nullptr) {
    return nullptr;
  }

  auto* block = BlockType::FromUsableSpace(ptr);
  while (!block->Last()) {
    block = block->Next();
    if (block->Used()) {
      return block->UsableSpace();
    }
  }
  return nullptr;
}

template <typename BlockAllocatorType>
void BlockAllocatorTest<BlockAllocatorType>::TearDown() {
  for (size_t i = 0; i < kNumPtrs; ++i) {
    if (Fetch(i) != nullptr) {
      allocator_.Deallocate(Fetch(i));
    }
  }
  allocator_.Reset();
}

// Unit tests template method implementations.

template <typename BlockAllocatorType>
void BlockAllocatorTest<BlockAllocatorType>::CanAutomaticallyInit(
    BlockAllocatorType& allocator) {
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

template <typename BlockAllocatorType>
void BlockAllocatorTest<BlockAllocatorType>::CanExplicitlyInit(
    BlockAllocatorType& allocator) {
  EXPECT_EQ(*(allocator.blocks().begin()), nullptr);
  EXPECT_EQ(allocator.Init(GetBytes()), pw::OkStatus());
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

template <typename BlockAllocatorType>
void BlockAllocatorTest<BlockAllocatorType>::IterateOverBlocks() {
  Allocator& allocator = GetAllocator({
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
  auto& block_allocator = static_cast<BlockAllocatorType&>(allocator);
  for (auto* block : block_allocator.blocks()) {
    if (block->Used()) {
      EXPECT_EQ(block->OuterSize(), kLargeOuterSize);
      ++used_count;
    } else {
      ++free_count;
    }
  }
  EXPECT_EQ(used_count, 3U);
  EXPECT_EQ(free_count, 4U);
}

template <typename BlockAllocatorType>
void BlockAllocatorTest<
    BlockAllocatorType>::CannotGetLayoutFromInvalidPointer() {
  Allocator& allocator = GetAllocator({
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

  auto& block_allocator = static_cast<BlockAllocatorType&>(allocator);
  for (const auto& block : block_allocator.blocks()) {
    if (!block->Used()) {
      Result<Layout> result1 = allocator.GetLayout(block->UsableSpace());
      EXPECT_EQ(result1.status(), Status::FailedPrecondition());
    }
  }
}

}  // namespace pw::allocator::test
