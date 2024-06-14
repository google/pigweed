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

#include "pw_allocator/block_allocator.h"
#include "pw_allocator/block_testing.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator::test {

/// Test fixture responsible for managing a memory region and an allocator that
/// allocates block of memory from it.
///
/// This base class contains all the code that does not depend specific
/// `Block` or `BlockAllocator` types.
class BlockAllocatorTestBase : public ::testing::Test {
 public:
  static constexpr size_t kDefaultBlockOverhead = Block<>::kBlockOverhead;

  // Size of the memory region to use in the tests below.
  // This must be large enough so that BlockType::Init does not fail.
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

  /// Swaps the pointer at indices `i` and `j`.
  void Swap(size_t i, size_t j);

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
  void CanMeasureFragmentation();

 private:
  BlockAllocatorType& allocator_;
  alignas(BlockType::kAlignment) std::array<std::byte, kCapacity> buffer_;
};

// Test fixture template method implementations.

template <typename BlockAllocatorType>
Allocator& BlockAllocatorTest<BlockAllocatorType>::GetAllocator() {
  allocator_.Init(GetBytes());
  return allocator_;
}

template <typename BlockAllocatorType>
Allocator& BlockAllocatorTest<BlockAllocatorType>::GetAllocator(
    std::initializer_list<Preallocation> preallocations) {
  auto* first = Preallocate<BlockType>(GetBytes(), preallocations);
  size_t index = 0;
  for (BlockType* block = first; block != nullptr; block = block->Next()) {
    Store(index, block->Used() ? block->UsableSpace() : nullptr);
    ++index;
  }
  allocator_.Init(first);
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
    void* ptr = Fetch(i);
    if (ptr != nullptr) {
      allocator_.Deallocate(ptr);
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
  allocator.Init(GetBytes());
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

template <typename BlockAllocatorType>
void BlockAllocatorTest<BlockAllocatorType>::IterateOverBlocks() {
  Allocator& allocator = GetAllocator({
      {kSmallOuterSize, Preallocation::kFree},
      {kLargeOuterSize, Preallocation::kUsed},
      {kSmallOuterSize, Preallocation::kFree},
      {kLargeOuterSize, Preallocation::kUsed},
      {kSmallOuterSize, Preallocation::kFree},
      {kLargeOuterSize, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kFree},
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
void BlockAllocatorTest<BlockAllocatorType>::CanMeasureFragmentation() {
  Allocator& allocator = GetAllocator({
      {0x020, Preallocation::kFree},
      {0x040, Preallocation::kUsed},
      {0x080, Preallocation::kFree},
      {0x100, Preallocation::kUsed},
      {0x200, Preallocation::kFree},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  auto& block_allocator = static_cast<BlockAllocatorType&>(allocator);
  constexpr size_t alignment = BlockAllocatorType::BlockType::kAlignment;
  size_t sum_of_squares = 0;
  size_t sum = 0;
  for (const auto block : block_allocator.blocks()) {
    if (!block->Used()) {
      size_t inner_size = block->InnerSize() / alignment;
      sum_of_squares += inner_size * inner_size;
      sum += inner_size;
    }
  }

  Fragmentation fragmentation = block_allocator.MeasureFragmentation();
  EXPECT_EQ(fragmentation.sum_of_squares.hi, 0U);
  EXPECT_EQ(fragmentation.sum_of_squares.lo, sum_of_squares);
  EXPECT_EQ(fragmentation.sum, sum);
}

}  // namespace pw::allocator::test
