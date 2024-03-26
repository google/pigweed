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

#include "pw_allocator/block_allocator.h"

#include <cstdint>
#include <limits>

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_allocator/buffer.h"
#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"
#include "pw_unit_test/framework_backend.h"

namespace pw::allocator {
namespace {

// Test fixtures.

using OffsetType = uint16_t;

// Size of the memory region to use in the tests below.
static constexpr size_t kCapacity = 1024;

// Represents the sizes of varios allocations.
static constexpr size_t kLargeInnerSize = kCapacity / 8;
static constexpr size_t kLargeOuterSize =
    Block<OffsetType>::kBlockOverhead + kLargeInnerSize;

static constexpr size_t kSmallInnerSize = Block<OffsetType>::kBlockOverhead * 2;
static constexpr size_t kSmallOuterSize =
    Block<OffsetType>::kBlockOverhead + kSmallInnerSize;

static constexpr size_t kSmallerOuterSize = kSmallInnerSize;
static constexpr size_t kLargerOuterSize = kLargeOuterSize + kSmallerOuterSize;

// Minimum size of a "large" allocation; allocation less than this size are
// considered "small" when using the DualFirstFit strategy.
static constexpr size_t kDualFitThreshold = kSmallInnerSize * 2;

// The number of allocated pointers cached by the test fixture.
static constexpr size_t kNumPtrs = 16;

/// Represents an initial state for a memory block.
///
/// Unit tests can specify an initial block layout by passing a list of these
/// structs to `Preallocate`.
///
/// The outer size of each block must be at least `kBlockOverhead` for the
/// block type in use. The special `kSizeRemaining` may be used for at most
/// one block to give it any space not assigned to other blocks.
///
/// The index must be less than `BlockAllocatorTestFixture::kNumPtrs` or one
/// of the special values `kIndexFree` or `kIndexNext`. A regular index will
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
  static constexpr size_t kIndexFree = kNumPtrs + 1;

  /// Special value indicating to use the next available index.
  static constexpr size_t kIndexNext = kNumPtrs + 2;
};

/// Test fixture responsible for managing a memory region and an allocator that
/// allocates from it.
template <typename BlockAllocatorType>
class TestFixture {
 public:
  using block_allocator_type = BlockAllocatorType;
  using BlockType = typename block_allocator_type::BlockType;

  TestFixture() { ptrs_.fill(nullptr); }

  ~TestFixture() {
    for (size_t i = 0; i < kNumPtrs; ++i) {
      if (ptrs_[i] != nullptr) {
        // `BlockAllocatorType::Deallocate` doesn't actually use the layout,
        // as the information it needs is encoded in the blocks.
        allocator_->Deallocate(ptrs_[i], Layout::Of<void*>());
      }
    }
    allocator_->Reset();
  }

  /// Returns the underlying memory region.
  ByteSpan bytes() { return ByteSpan(allocator_.data(), allocator_.size()); }

  /// Returns a references to a cached pointer.
  void*& operator[](size_t index) { return ptrs_[index]; }

  /// Initialize the allocator with a region of memory and return it.
  block_allocator_type& GetAllocator() {
    EXPECT_EQ(allocator_->Init(bytes()), OkStatus());
    return *allocator_;
  }

  /// Initialize the allocator with a sequence of preallocated blocks and return
  /// it.
  ///
  /// See also ``Preallocation``.
  block_allocator_type& GetAllocator(
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

    Result<BlockType*> result = BlockType::Init(bytes());
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
          if (ptrs_[next_index] == nullptr &&
              std::all_of(preallocations.begin(),
                          preallocations.end(),
                          [next_index](const Preallocation& other) {
                            return other.index != next_index;
                          })) {
            break;
          }
          ++next_index;
        }
        ptrs_[next_index] = block->UsableSpace();

      } else {
        ptrs_[preallocation.index] = block->UsableSpace();
      }
      block = block->Next();
    }
    if (block != nullptr) {
      block->MarkFree();
    }
    block = BlockType::FromUsableSpace(begin);
    PW_CHECK_OK(allocator_->Init(block));
    return *allocator_;
  }

  /// Gets the next allocation from an allocated pointer.
  void* NextAfter(size_t index) {
    if (index > ptrs_.size()) {
      return nullptr;
    }
    auto* block = BlockType::FromUsableSpace(ptrs_[index]);
    while (!block->Last()) {
      block = block->Next();
      if (block->Used()) {
        return block->UsableSpace();
      }
    }
    return nullptr;
  }

 private:
  WithBuffer<BlockAllocatorType, kCapacity, BlockType::kAlignment> allocator_;
  std::array<void*, kNumPtrs> ptrs_;
};

/// Ensures the memory is usable by writing to it.
void UseMemory(void* ptr, size_t size) { std::memset(ptr, 0x5a, size); }

#define TEST_ONE_STRATEGY(strategy, test_case)                      \
  TEST(BlockAllocatorTest, test_case##_##strategy) {                \
    TestFixture<strategy##BlockAllocator<OffsetType>> test_fixture; \
    test_case(test_fixture);                                        \
  }

#define TEST_FOREACH_STRATEGY(test_case) \
  TEST_ONE_STRATEGY(FirstFit, test_case) \
  TEST_ONE_STRATEGY(LastFit, test_case)  \
  TEST_ONE_STRATEGY(BestFit, test_case)  \
  TEST_ONE_STRATEGY(WorstFit, test_case) \
  TEST_ONE_STRATEGY(DualFirstFit, test_case)

// Unit tests.

template <typename TestFixtureType>
void CanAutomaticallyInit(TestFixtureType& test_fixture) {
  typename TestFixtureType::block_allocator_type allocator(
      test_fixture.bytes());
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}
TEST_ONE_STRATEGY(FirstFit, CanAutomaticallyInit)
TEST_ONE_STRATEGY(LastFit, CanAutomaticallyInit)
TEST_ONE_STRATEGY(BestFit, CanAutomaticallyInit)
TEST_ONE_STRATEGY(WorstFit, CanAutomaticallyInit)
TEST(BlockAllocatorTest, CanAutomaticallyInit_DualFirstFit) {
  std::array<std::byte, kCapacity> buffer;
  ByteSpan bytes(buffer);
  DualFirstFitBlockAllocator<OffsetType> allocator(bytes, kDualFitThreshold);
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

template <typename TestFixtureType>
void CanExplicitlyInit(TestFixtureType& test_fixture) {
  typename TestFixtureType::block_allocator_type allocator;
  EXPECT_EQ(*(allocator.blocks().begin()), nullptr);
  EXPECT_EQ(allocator.Init(test_fixture.bytes()), OkStatus());
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}
TEST_FOREACH_STRATEGY(CanExplicitlyInit)

template <typename TestFixtureType>
void GetCapacity(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  StatusWithSize capacity = allocator.GetCapacity();
  EXPECT_EQ(capacity.status(), OkStatus());
  EXPECT_EQ(capacity.size(), kCapacity);
}

template <typename TestFixtureType>
void AllocateLarge(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kLargeInnerSize]>();
  test_fixture[0] = allocator.Allocate(layout);
  ASSERT_NE(test_fixture[0], nullptr);
  ByteSpan bytes = test_fixture.bytes();
  EXPECT_GE(test_fixture[0], bytes.data());
  EXPECT_LE(test_fixture[0], bytes.data() + bytes.size());
  UseMemory(test_fixture[0], layout.size());
}
TEST_FOREACH_STRATEGY(AllocateLarge)

template <typename TestFixtureType>
void AllocateSmall(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();
  test_fixture[0] = allocator.Allocate(layout);
  ASSERT_NE(test_fixture[0], nullptr);
  ByteSpan bytes = test_fixture.bytes();
  EXPECT_GE(test_fixture[0], bytes.data());
  EXPECT_LE(test_fixture[0], bytes.data() + bytes.size());
  UseMemory(test_fixture[0], layout.size());
}
TEST_FOREACH_STRATEGY(AllocateSmall)

template <typename TestFixtureType>
void AllocateTooLarge(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  test_fixture[0] = allocator.Allocate(Layout::Of<std::byte[kCapacity * 2]>());
  EXPECT_EQ(test_fixture[0], nullptr);
}
TEST_FOREACH_STRATEGY(AllocateTooLarge)

template <typename TestFixtureType>
void AllocateLargeAlignment(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr size_t kAlignment = 64;
  test_fixture[0] = allocator.Allocate(Layout(kLargeInnerSize, kAlignment));
  ASSERT_NE(test_fixture[0], nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(test_fixture[0]) % kAlignment, 0U);
  UseMemory(test_fixture[0], kLargeInnerSize);

  test_fixture[1] = allocator.Allocate(Layout(kLargeInnerSize, kAlignment));
  ASSERT_NE(test_fixture[1], nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(test_fixture[1]) % kAlignment, 0U);
  UseMemory(test_fixture[1], kLargeInnerSize);
}
TEST_FOREACH_STRATEGY(AllocateLargeAlignment)

template <typename TestFixtureType>
void AllocateAlignmentFailure(TestFixtureType& test_fixture) {
  // Allocate a two blocks with an unaligned region between them.
  using BlockType = typename TestFixtureType::BlockType;
  constexpr size_t kAlignment = 128;
  ByteSpan bytes = test_fixture.bytes();
  auto addr = reinterpret_cast<uintptr_t>(bytes.data());
  uintptr_t outer_size =
      AlignUp(addr + BlockType::kBlockOverhead, kAlignment) - addr + 1;
  auto& allocator = test_fixture.GetAllocator({
      {outer_size, 0},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 2},
  });

  // The allocator should be unable to create an aligned region..
  test_fixture[1] = allocator.Allocate(Layout(kLargeInnerSize, kAlignment));
  EXPECT_EQ(test_fixture[1], nullptr);
}
TEST_FOREACH_STRATEGY(AllocateAlignmentFailure)

TEST(FirstFit, AllocatesFirstCompatible) {
  TestFixture<FirstFitBlockAllocator<OffsetType>> test_fixture;
  auto& allocator = test_fixture.GetAllocator({
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 3},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 5},
  });

  test_fixture[0] = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(0), test_fixture[1]);
  test_fixture[4] = allocator.Allocate(Layout(kLargeInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(3), test_fixture[4]);
  EXPECT_EQ(test_fixture.NextAfter(4), test_fixture[5]);
}

TEST(LastFit, AllocatesLastCompatible) {
  TestFixture<LastFitBlockAllocator<OffsetType>> test_fixture;
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 3},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 5},
  });

  test_fixture[0] = allocator.Allocate(Layout(kLargeInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(0), test_fixture[1]);
  test_fixture[4] = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(3), test_fixture[4]);
  EXPECT_EQ(test_fixture.NextAfter(4), test_fixture[5]);
}

TEST(BestFit, AllocatesBestCompatible) {
  TestFixture<BestFitBlockAllocator<OffsetType>> test_fixture;
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 3},
      {kSmallerOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 5},
      {kLargerOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 7},
  });

  test_fixture[2] = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(1), test_fixture[2]);
  EXPECT_EQ(test_fixture.NextAfter(2), test_fixture[3]);
  test_fixture[0] = allocator.Allocate(Layout(kLargeInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(0), test_fixture[1]);
}

TEST(WorstFit, AllocatesWorstCompatible) {
  TestFixture<WorstFitBlockAllocator<OffsetType>> test_fixture;
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 3},
      {kSmallerOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 5},
      {kLargerOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 7},
  });

  test_fixture[6] = allocator.Allocate(Layout(kLargeInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(5), test_fixture[6]);
  EXPECT_EQ(test_fixture.NextAfter(6), test_fixture[7]);
  test_fixture[0] = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(0), test_fixture[1]);
}

TEST(DualFirstFit, AllocatesUsingThreshold) {
  TestFixture<DualFirstFitBlockAllocator<OffsetType>> test_fixture;
  auto& allocator = test_fixture.GetAllocator({
      {kLargerOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 3},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 5},
      {kSmallOuterSize, Preallocation::kIndexFree},
  });
  allocator.set_threshold(kDualFitThreshold);

  test_fixture[0] = allocator.Allocate(Layout(kLargeInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(0), test_fixture[1]);
  test_fixture[4] = allocator.Allocate(Layout(kLargeInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(3), test_fixture[4]);
  EXPECT_EQ(test_fixture.NextAfter(4), test_fixture[5]);
  test_fixture[6] = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(5), test_fixture[6]);
  EXPECT_EQ(test_fixture.NextAfter(6), test_fixture[7]);
  test_fixture[2] = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_EQ(test_fixture.NextAfter(1), test_fixture[2]);
  EXPECT_EQ(test_fixture.NextAfter(2), test_fixture[3]);
}

template <typename TestFixtureType>
void DeallocateNull(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout = Layout::Of<uint8_t>();
  allocator.Deallocate(nullptr, layout);
}
TEST_FOREACH_STRATEGY(DeallocateNull)

template <typename TestFixtureType>
void DeallocateShuffled(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();
  for (size_t i = 0; i < kNumPtrs; ++i) {
    test_fixture[i] = allocator.Allocate(layout);
    if (test_fixture[i] == nullptr) {
      break;
    }
  }

  // Mix up the order of allocations.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    if (i % 2 == 0 && i + 1 < kNumPtrs) {
      std::swap(test_fixture[i], test_fixture[i + 1]);
    }
    if (i % 3 == 0 && i + 2 < kNumPtrs) {
      std::swap(test_fixture[i], test_fixture[i + 2]);
    }
  }

  // Deallocate everything.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    allocator.Deallocate(test_fixture[i], layout);
    test_fixture[i] = nullptr;
  }
}
TEST_FOREACH_STRATEGY(DeallocateShuffled)

TEST(BlockAllocatorTest, DisablePoisoning) {
  using BlockAllocatorType = FirstFitBlockAllocator<OffsetType, 0>;
  using BlockType = BlockAllocatorType::BlockType;
  TestFixture<BlockAllocatorType> test_fixture;
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();

  // Create a bunch of blocks
  for (size_t i = 0; i < kNumPtrs; ++i) {
    test_fixture[i] = allocator.Allocate(layout);
    ASSERT_NE(test_fixture[i], nullptr);
  }
  for (size_t i = 0; i < kNumPtrs; i += 2) {
    uint8_t* ptr = static_cast<uint8_t*>(test_fixture[i]);
    test_fixture[i] = nullptr;

    // Free every other to prevent merging.
    allocator.Deallocate(ptr, layout);

    // Modify the contents of the block and check if it is still valid.
    auto* block = BlockType::FromUsableSpace(ptr);
    EXPECT_FALSE(block->Used());
    EXPECT_TRUE(block->IsValid());
    ptr[0] = ~ptr[0];
    EXPECT_TRUE(block->IsValid());
  }
}

TEST(BlockAllocatorTest, PoisonEveryFreeBlock) {
  using BlockAllocatorType = FirstFitBlockAllocator<OffsetType, 1>;
  using BlockType = BlockAllocatorType::BlockType;
  TestFixture<BlockAllocatorType> test_fixture;
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();

  // Create a bunch of blocks
  for (size_t i = 0; i < kNumPtrs; ++i) {
    test_fixture[i] = allocator.Allocate(layout);
    ASSERT_NE(test_fixture[i], nullptr);
  }
  for (size_t i = 0; i < kNumPtrs; i += 2) {
    uint8_t* ptr = static_cast<uint8_t*>(test_fixture[i]);
    test_fixture[i] = nullptr;

    // Free every other to prevent merging.
    allocator.Deallocate(ptr, layout);

    // Modify the contents of the block and check if it is still valid.
    auto* block = BlockType::FromUsableSpace(ptr);
    EXPECT_FALSE(block->Used());
    EXPECT_TRUE(block->IsValid());
    ptr[0] = ~ptr[0];
    EXPECT_FALSE(block->IsValid());
  }
}

TEST(BlockAllocatorTest, PoisonPeriodically) {
  using BlockAllocatorType = FirstFitBlockAllocator<OffsetType, 4>;
  using BlockType = BlockAllocatorType::BlockType;
  TestFixture<BlockAllocatorType> test_fixture;
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();

  // Create a bunch of blocks
  for (size_t i = 0; i < kNumPtrs; ++i) {
    test_fixture[i] = allocator.Allocate(layout);
    ASSERT_NE(test_fixture[i], nullptr);
  }
  for (size_t i = 0; i < kNumPtrs; i += 2) {
    uint8_t* ptr = static_cast<uint8_t*>(test_fixture[i]);
    test_fixture[i] = nullptr;

    // Free every other to prevent merging.
    allocator.Deallocate(ptr, layout);

    // Modify the contents of the block and check if it is still valid.
    auto* block = BlockType::FromUsableSpace(ptr);
    EXPECT_FALSE(block->Used());
    EXPECT_TRUE(block->IsValid());
    ptr[0] = ~ptr[0];
    if ((i / 2) % 4 == 3) {
      EXPECT_FALSE(block->IsValid());
    } else {
      EXPECT_TRUE(block->IsValid());
    }
  }
}

template <typename TestFixtureType>
void IterateOverBlocks(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
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
TEST_FOREACH_STRATEGY(IterateOverBlocks)

template <typename TestFixtureType>
void QueryLargeValid(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kSmallOuterSize, 0},
      {kLargeOuterSize, 1},
      {kSmallOuterSize, 2},
  });

  Layout layout(kLargeInnerSize, 1);
  EXPECT_EQ(allocator.Query(test_fixture[1], layout), OkStatus());
}
TEST_FOREACH_STRATEGY(QueryLargeValid)

template <typename TestFixtureType>
void QuerySmallValid(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, 0},
      {kSmallOuterSize, 1},
      {kLargeOuterSize, 2},
  });
  EXPECT_EQ(allocator.Query(test_fixture[1], Layout(kSmallInnerSize, 1)),
            OkStatus());
}
TEST_FOREACH_STRATEGY(QuerySmallValid)

template <typename TestFixtureType>
void QueryInvalidPtr(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout layout =
      Layout::Of<typename TestFixtureType::block_allocator_type>();
  EXPECT_EQ(allocator.Query(&allocator, layout), Status::OutOfRange());
}
TEST_FOREACH_STRATEGY(QueryInvalidPtr)

template <typename TestFixtureType>
void ResizeNull(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr Layout old_layout = Layout::Of<uint8_t>();
  size_t new_size = 1;
  EXPECT_FALSE(allocator.Resize(nullptr, old_layout, new_size));
}
TEST_FOREACH_STRATEGY(ResizeNull)

template <typename TestFixtureType>
void ResizeLargeSame(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, 0},
      {kLargeOuterSize, 1},
  });
  Layout old_layout(kLargeInnerSize, 1);
  size_t new_size = kLargeInnerSize;
  ASSERT_TRUE(allocator.Resize(test_fixture[0], old_layout, new_size));
  UseMemory(test_fixture[0], kLargeInnerSize);
}
TEST_FOREACH_STRATEGY(ResizeLargeSame)

template <typename TestFixtureType>
void ResizeLargeSmaller(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, 0},
      {kLargeOuterSize, 1},
  });
  Layout old_layout(kLargeInnerSize, 1);
  size_t new_size = kSmallInnerSize;
  ASSERT_TRUE(allocator.Resize(test_fixture[0], old_layout, new_size));
  UseMemory(test_fixture[0], kSmallInnerSize);
}
TEST_FOREACH_STRATEGY(ResizeLargeSmaller)

template <typename TestFixtureType>
void ResizeLargeLarger(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, 0},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallOuterSize, 2},
  });
  Layout old_layout(kLargeInnerSize, 1);
  size_t new_size = kLargeInnerSize * 2;
  ASSERT_TRUE(allocator.Resize(test_fixture[0], old_layout, new_size));
  UseMemory(test_fixture[0], kLargeInnerSize * 2);
}
TEST_FOREACH_STRATEGY(ResizeLargeLarger)

template <typename TestFixtureType>
void ResizeLargeLargerFailure(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kLargeOuterSize, 0},
      {kSmallOuterSize, 12},
  });
  // Memory after ptr is already allocated, so `Resize` should fail.
  Layout old_layout(kLargeInnerSize, 1);
  size_t new_size = kLargeInnerSize * 2;
  EXPECT_FALSE(allocator.Resize(test_fixture[0], old_layout, new_size));
}
TEST_FOREACH_STRATEGY(ResizeLargeLargerFailure)

template <typename TestFixtureType>
void ResizeSmallSame(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, 1},
  });
  Layout old_layout(kSmallInnerSize, 1);
  size_t new_size = kSmallInnerSize;
  ASSERT_TRUE(allocator.Resize(test_fixture[0], old_layout, new_size));
  UseMemory(test_fixture[0], kSmallInnerSize);
}
TEST_FOREACH_STRATEGY(ResizeSmallSame)

template <typename TestFixtureType>
void ResizeSmallSmaller(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, 1},
  });
  Layout old_layout(kSmallInnerSize, 1);
  size_t new_size = kSmallInnerSize / 2;
  ASSERT_TRUE(allocator.Resize(test_fixture[0], old_layout, new_size));
  UseMemory(test_fixture[0], kSmallInnerSize / 2);
}
TEST_FOREACH_STRATEGY(ResizeSmallSmaller)

template <typename TestFixtureType>
void ResizeSmallLarger(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallOuterSize, 2},
  });
  Layout old_layout(kSmallInnerSize, 1);
  size_t new_size = kSmallInnerSize * 2;
  ASSERT_TRUE(allocator.Resize(test_fixture[0], old_layout, new_size));
  UseMemory(test_fixture[0], kSmallInnerSize * 2);
}
TEST_FOREACH_STRATEGY(ResizeSmallLarger)

template <typename TestFixtureType>
void ResizeSmallLargerFailure(TestFixtureType& test_fixture) {
  using BlockType = typename TestFixtureType::BlockType;
  auto& allocator = test_fixture.GetAllocator({
      {kSmallOuterSize, 0},
      {kSmallOuterSize, 1},
  });
  // Memory after ptr is already allocated, so `Resize` should fail.
  Layout old_layout(kSmallInnerSize, 1);
  size_t new_size = kSmallInnerSize * 2 + BlockType::kBlockOverhead;
  EXPECT_FALSE(allocator.Resize(test_fixture[0], old_layout, new_size));
}
TEST_FOREACH_STRATEGY(ResizeSmallLargerFailure)

TEST(DualFirstFit, ResizeLargeSmallerAcrossThreshold) {
  TestFixture<DualFirstFitBlockAllocator<OffsetType>> test_fixture;
  auto& allocator = test_fixture.GetAllocator({{kDualFitThreshold * 2, 0}});
  // Shrinking succeeds, and the pointer is unchanged even though it is now
  // below the threshold.
  Layout old_layout(kDualFitThreshold * 2, 1);
  size_t new_size = kDualFitThreshold / 2;
  ASSERT_TRUE(allocator.Resize(test_fixture[0], old_layout, new_size));
  UseMemory(test_fixture[0], kDualFitThreshold / 2);
}

TEST(DualFirstFit, ResizeSmallLargerAcrossThreshold) {
  TestFixture<DualFirstFitBlockAllocator<OffsetType>> test_fixture;
  auto& allocator = test_fixture.GetAllocator({
      {Preallocation::kSizeRemaining, Preallocation::kIndexNext},
      {kDualFitThreshold / 2, 1},
      {kDualFitThreshold * 2, Preallocation::kIndexFree},
  });
  // Growing succeeds, and the pointer is unchanged even though it is now
  // above the threshold.
  Layout old_layout(kDualFitThreshold / 2, 1);
  size_t new_size = kDualFitThreshold * 2;
  ASSERT_TRUE(allocator.Resize(test_fixture[1], old_layout, new_size));
  UseMemory(test_fixture[1], kDualFitThreshold * 2);
}

template <typename TestFixtureType>
void CanGetLayoutFromValidPointer(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator();
  constexpr size_t kAlignment = 64;
  test_fixture[0] = allocator.Allocate(Layout(kLargeInnerSize, kAlignment * 2));
  ASSERT_NE(test_fixture[0], nullptr);

  test_fixture[1] = allocator.Allocate(Layout(kSmallInnerSize, kAlignment / 2));
  ASSERT_NE(test_fixture[1], nullptr);

  Result<Layout> result0 = allocator.GetLayout(test_fixture[0]);
  ASSERT_EQ(result0.status(), OkStatus());
  EXPECT_GE(result0->size(), kLargeInnerSize);
  EXPECT_EQ(result0->alignment(), kAlignment * 2);

  Result<Layout> result1 = allocator.GetLayout(test_fixture[1]);
  ASSERT_EQ(result1.status(), OkStatus());
  EXPECT_GE(result1->size(), kSmallInnerSize);
  EXPECT_EQ(result1->alignment(), kAlignment / 2);
}
TEST_FOREACH_STRATEGY(CanGetLayoutFromValidPointer)

template <typename TestFixtureType>
void CannotGetLayoutFromInvalidPointer(TestFixtureType& test_fixture) {
  auto& allocator = test_fixture.GetAllocator({
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
TEST_FOREACH_STRATEGY(CannotGetLayoutFromInvalidPointer)

}  // namespace
}  // namespace pw::allocator
