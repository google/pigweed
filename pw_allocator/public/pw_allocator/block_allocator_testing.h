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
#include "pw_unit_test/framework.h"

namespace pw::allocator::test {

// Forward declaration.
struct Preallocation;

/// Test fixture responsible for managing a memory region and an allocator that
/// allocates from it.
class BlockAllocatorTest : public ::testing::Test {
 public:
  using OffsetType = uint16_t;
  using BlockType = Block<OffsetType>;
  using BlockAllocatorType = BlockAllocator<OffsetType, 0, alignof(OffsetType)>;

  static_assert(
      std::is_same_v<BlockType, typename BlockAllocatorType::BlockType>);

  // Size of the memory region to use in the tests below.
  static constexpr size_t kCapacity = 1024;

  // The number of allocated pointers cached by the test fixture.
  static constexpr size_t kNumPtrs = 16;

  // Represents the sizes of varios allocations.
  static constexpr size_t kLargeInnerSize = kCapacity / 8;
  static constexpr size_t kLargeOuterSize =
      BlockType::kBlockOverhead + kLargeInnerSize;

  static constexpr size_t kSmallInnerSize = BlockType::kBlockOverhead * 2;
  static constexpr size_t kSmallOuterSize =
      BlockType::kBlockOverhead + kSmallInnerSize;

  static constexpr size_t kSmallerOuterSize = kSmallInnerSize;
  static constexpr size_t kLargerOuterSize =
      kLargeOuterSize + kSmallerOuterSize;

 protected:
  BlockAllocatorTest(BlockAllocatorType& allocator);

  // Test fixtures.

  void SetUp() override;

  /// Returns the underlying memory region.
  ByteSpan GetBytes();

  /// Initialize the allocator with a region of memory and return it.
  BlockAllocatorType& GetAllocator();

  /// Initialize the allocator with a sequence of preallocated blocks and return
  /// it.
  ///
  /// See also ``Preallocation``.
  BlockAllocatorType& GetAllocator(
      std::initializer_list<Preallocation> preallocations);

  /// Gets the next allocation from an allocated pointer.
  void* NextAfter(size_t index);

  /// Store an allocated pointer in the test's cache of pointers.
  void Store(size_t index, void* ptr);

  /// Retrieve an allocated pointer from the test's cache of pointers.
  void* Fetch(size_t index);

  /// Ensures the memory is usable by writing to it.
  void UseMemory(void* ptr, size_t size);

  void TearDown() override;

  // Unit tests.
  void CanAutomaticallyInit(BlockAllocatorType& allocator);
  void CanExplicitlyInit(BlockAllocatorType& allocator);
  void GetCapacity();
  void AllocateLarge();
  void AllocateSmall();
  void AllocateTooLarge();
  void AllocateLargeAlignment();
  void AllocateAlignmentFailure();
  void DeallocateNull();
  void DeallocateShuffled();
  void IterateOverBlocks();
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
  void CannotGetLayoutFromInvalidPointer();

 private:
  BlockAllocatorType& allocator_;
  alignas(BlockType::kAlignment) std::array<std::byte, kCapacity> buffer_;
  std::array<void*, kNumPtrs> ptrs_;
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
  static constexpr size_t kIndexFree = BlockAllocatorTest::kNumPtrs + 1;

  /// Special value indicating to use the next available index.
  static constexpr size_t kIndexNext = BlockAllocatorTest::kNumPtrs + 2;
};

}  // namespace pw::allocator::test
