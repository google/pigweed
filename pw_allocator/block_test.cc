// Copyright 2020 The Pigweed Authors
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

#include "pw_allocator/block.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "pw_allocator/block_testing.h"
#include "pw_assert/check.h"
#include "pw_assert/internal/check_impl.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.
using ::pw::allocator::BlockAllocType;
using ::pw::allocator::Layout;
using ::pw::allocator::test::Preallocate;
using ::pw::allocator::test::Preallocation;

// The size of the memory region used in tests.
constexpr size_t kN = 1024;

// The large alignment used in alignment-related tests.
constexpr size_t kAlign = 64;

template <typename BlockType>
class BlockTest : public ::testing::Test {
 protected:
  // Some tests below need a block with a nonzero inner size to fit within
  // alignment boundaries.
  static_assert(kAlign > BlockType::kBlockOverhead + BlockType::kAlignment);

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes_;
};

// A block type with more overhead.
using LargeOffsetBlock = ::pw::allocator::Block<uint64_t>;

class LargeOffsetBlockTest : public BlockTest<LargeOffsetBlock> {
 protected:
  using BlockType = LargeOffsetBlock;
};

// A block type with less overhead.
using SmallOffsetBlock = ::pw::allocator::Block<uint16_t>;

class SmallOffsetBlockTest : public BlockTest<SmallOffsetBlock> {
 protected:
  using BlockType = SmallOffsetBlock;
};

// A block type with moderate overhead and support for poisoning.
using PoisonedBlock = ::pw::allocator::Block<uint32_t, alignof(uint32_t), true>;

class PoisonedBlockTest : public BlockTest<PoisonedBlock> {
 protected:
  using BlockType = PoisonedBlock;
};

/// Returns the smallest offset into the given memory region which can be
/// preceded by a valid block, and at which a block would have properly aligned
/// usable space of the given size.
///
/// @pre ``bytes`` must not be smaller than the calculated offset plus
///      ``layout.size()``.
template <typename BlockType>
size_t GetFirstAlignedOffset(pw::ByteSpan bytes, Layout layout) {
  auto start = reinterpret_cast<uintptr_t>(bytes.data());
  uintptr_t end = start + bytes.size();
  size_t alignment = std::max(BlockType::kAlignment, layout.alignment());

  // Find the minimum address of the usable space of the second block.
  size_t min_addr = start;
  PW_CHECK_ADD(min_addr, BlockType::kBlockOverhead + 1, &min_addr);
  PW_CHECK_ADD(min_addr, BlockType::kBlockOverhead, &min_addr);

  // Align the usable space and ensure it fits.
  uintptr_t addr = pw::AlignUp(min_addr, alignment);
  PW_CHECK_UINT_LE(addr + layout.size(), end);

  // Return the offset to the start of the block.
  PW_CHECK_SUB(addr, BlockType::kBlockOverhead, &addr);
  return addr - start;
}

/// Returns the largest offset into the given memory region at which a block
/// would have properly aligned usable space of the given size.
///
/// @pre ``bytes`` must not be smaller than the calculated offset plus
///      ``layout.size()``.
template <typename BlockType>
size_t GetLastAlignedOffset(pw::ByteSpan bytes, Layout layout) {
  auto start = reinterpret_cast<uintptr_t>(bytes.data());
  uintptr_t end = start + bytes.size();
  size_t alignment = std::max(BlockType::kAlignment, layout.alignment());

  // Find the minimum address of the usable space of the second block.
  size_t min_addr = start;
  PW_CHECK_ADD(min_addr, BlockType::kBlockOverhead + 1, &min_addr);
  PW_CHECK_ADD(min_addr, BlockType::kBlockOverhead, &min_addr);

  // Align the usable space and ensure it fits.
  uintptr_t addr;
  PW_CHECK_SUB(end, layout.size(), &addr);
  addr = pw::AlignDown(addr, alignment);
  PW_CHECK_UINT_LE(min_addr, addr);

  // Return the offset to the start of the block.
  PW_CHECK_SUB(addr, BlockType::kBlockOverhead, &addr);
  return addr - start;
}

// Macro to provide type-parameterized tests for the various block types above.
//
// Ideally, the unit tests below could just use `TYPED_TEST_P` and its
// asscoiated macros from GoogleTest, see
// https://github.com/google/googletest/blob/main/docs/advanced.md#type-parameterized-tests
//
// These macros are not supported by the light framework however, so this macro
// provides a custom implementation that works just for these types.
#define TEST_FOR_EACH_BLOCK_TYPE(TestCase)                                 \
  template <typename BlockType>                                            \
  void TestCase(pw::ByteSpan bytes_);                                      \
  TEST_F(LargeOffsetBlockTest, TestCase) {                                 \
    TestCase<LargeOffsetBlock>(bytes_);                                    \
  }                                                                        \
  TEST_F(SmallOffsetBlockTest, TestCase) {                                 \
    TestCase<SmallOffsetBlock>(bytes_);                                    \
  }                                                                        \
  TEST_F(PoisonedBlockTest, TestCase) { TestCase<PoisonedBlock>(bytes_); } \
  template <typename BlockType>                                            \
  void TestCase([[maybe_unused]] pw::ByteSpan bytes_)

// Unit tests.

TEST_FOR_EACH_BLOCK_TYPE(CanCreateSingleAlignedBlock) {
  auto result = BlockType::Init(bytes_);
  ASSERT_EQ(result.status(), pw::OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(block->OuterSize(), kN);
  EXPECT_EQ(block->InnerSize(), kN - BlockType::kBlockOverhead);
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_EQ(block->Next(), nullptr);
  EXPECT_FALSE(block->Used());
  EXPECT_TRUE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanCreateUnalignedSingleBlock) {
  pw::ByteSpan aligned(bytes_);

  auto result = BlockType::Init(aligned.subspan(1));
  EXPECT_EQ(result.status(), pw::OkStatus());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotCreateTooSmallBlock) {
  std::array<std::byte, 2> bytes;
  auto result = BlockType::Init(bytes);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
}

TEST(BlockTest, CannotCreateTooLargeBlock) {
  std::array<std::byte, kN> bytes;
  auto result = pw::allocator::Block<uint8_t>::Init(bytes);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst) {
  constexpr Layout kLayout(256, kAlign);

  // Make sure the block's usable space is aligned.
  size_t outer_size = GetFirstAlignedOffset<BlockType>(bytes_, kLayout);
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {outer_size, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  block = block->Next();

  // Allocate from the front of the block.
  BlockType* prev = block->Prev();
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, BlockAllocType::kNewNext);

  EXPECT_EQ(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  // No new padding block was allocated.
  EXPECT_EQ(prev, block->Prev());

  // Extra was split from the end of the block.
  EXPECT_FALSE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirstWithNewPrevBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Make sure the block's usable space is not aligned.
  size_t outer_size = GetFirstAlignedOffset<BlockType>(bytes_, kLayout);
  outer_size += BlockType::kAlignment;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {outer_size, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  block = block->Next();
  BlockType* prev = block->Prev();

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, BlockAllocType::kNewPrevAndNewNext);

  EXPECT_EQ(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  // A new free block was added.
  EXPECT_LT(prev, block->Prev());
  EXPECT_FALSE(block->Prev()->Used());

  // Extra was split from the end of the block.
  EXPECT_FALSE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirstWithNoNewNextBlock) {
  constexpr Layout kLayout(256, kAlign);
  constexpr size_t kOuterSize = BlockType::kBlockOverhead + kLayout.size();

  // Make sure the block's usable space is aligned.
  size_t outer_size = GetFirstAlignedOffset<BlockType>(bytes_, kLayout);
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {outer_size, Preallocation::kUsed},
          {kOuterSize, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();
  BlockType* next = block->Next();

  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, BlockAllocType::kExact);

  EXPECT_EQ(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  // No new trailing block was created.
  EXPECT_EQ(next, block->Next());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirstWithResizedPrevBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Make sure the block's usable space is not aligned.
  size_t outer_size = GetFirstAlignedOffset<BlockType>(bytes_, kLayout);
  outer_size +=
      pw::AlignUp(BlockType::kBlockOverhead, kAlign) - BlockType::kAlignment;
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {outer_size, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  block = block->Next();
  BlockType* prev = block->Prev();
  size_t prev_inner_size = prev->InnerSize();

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, BlockAllocType::kShiftToPrevAndNewNext);

  EXPECT_EQ(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  /// Less than a minimum block was added to the previous block.
  EXPECT_EQ(prev, block->Prev());
  EXPECT_EQ(prev->InnerSize() - prev_inner_size, BlockType::kAlignment);

  // Extra was split from the end of the block.
  EXPECT_FALSE(block->Last());

  // On freeing the block, the previous block goes back to its original size.
  BlockType::Free(block);
  EXPECT_EQ(prev->InnerSize(), prev_inner_size);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirstIfTooSmallForAlignment) {
  constexpr Layout kLayout(256, kAlign);
  constexpr size_t kOuterSize = BlockType::kBlockOverhead + kLayout.size();

  // Make sure the block's usable space is not aligned.
  size_t outer_size = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + 1;
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {outer_size, Preallocation::kUsed},
          {kOuterSize, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Cannot allocate without room to a split a block for alignment.
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirstFromNull) {
  BlockType* block = nullptr;
  constexpr Layout kLayout(1, 1);
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirstZeroSize) {
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  constexpr Layout kLayout(0, 1);
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirstFromUsed) {
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  constexpr Layout kLayout(1, 1);
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::FailedPrecondition());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLastWithNewPrevBlock) {
  constexpr Layout kLayout(256, kAlign);

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  // Allocate from the back of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, BlockAllocType::kNewPrev);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  // Extra was split from the front of the block.
  EXPECT_FALSE(block->Prev()->Used());
  EXPECT_TRUE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLastWithResizedPrevBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Make sure the block's usable space is not aligned.
  size_t outer_size = GetLastAlignedOffset<BlockType>(bytes_, kLayout);
  outer_size -= BlockType::kAlignment;
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {outer_size, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  block = block->Next();
  BlockType* next = block->Next();
  BlockType* prev = block->Prev();
  size_t prev_inner_size = prev->InnerSize();

  // Allocate from the back of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, BlockAllocType::kShiftToPrev);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  /// Less than a minimum block was added to the previous block.
  EXPECT_EQ(prev, block->Prev());
  EXPECT_EQ(prev->InnerSize() - prev_inner_size, BlockType::kAlignment);

  // No new trailing block was created.
  EXPECT_EQ(next, block->Next());

  // On freeing the block, the previous block goes back to its original size.
  BlockType::Free(block);
  EXPECT_EQ(prev->InnerSize(), prev_inner_size);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLastIfTooSmallForAlignment) {
  constexpr Layout kLayout(256, kAlign);
  constexpr size_t kOuterSize = BlockType::kBlockOverhead + kLayout.size();

  // Make sure the block's usable space is not aligned.
  size_t outer_size = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + 1;
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {outer_size, Preallocation::kUsed},
          {kOuterSize, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Cannot allocate without room to a split a block for alignment.
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLastFromNull) {
  BlockType* block = nullptr;
  constexpr Layout kLayout(1, 1);
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLastZeroSize) {
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  constexpr Layout kLayout(0, 1);
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLastFromUsed) {
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  constexpr Layout kLayout(1, 1);
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::FailedPrecondition());
}

TEST_FOR_EACH_BLOCK_TYPE(FreeingNullDoesNothing) {
  BlockType* block = nullptr;
  BlockType::Free(block);
}

TEST_FOR_EACH_BLOCK_TYPE(FreeingFreeBlockDoesNothing) {
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  BlockType::Free(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanFree) {
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(block->OuterSize(), kN);
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockWithoutMerging) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();
  BlockType* next = block->Next();
  BlockType* prev = block->Prev();

  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(next, block->Next());
  EXPECT_EQ(prev, block->Prev());
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithPrev) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kFree},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();
  BlockType* next = block->Next();

  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_EQ(block->Next(), next);
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithNext) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  block = block->Next();
  BlockType* prev = block->Prev();

  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(block->Prev(), prev);
  EXPECT_TRUE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithBoth) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kFree},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  block = block->Next();

  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_TRUE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockSameSize) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  EXPECT_EQ(BlockType::Resize(block, block->InnerSize()), pw::OkStatus());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeFreeBlock) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  EXPECT_EQ(BlockType::Resize(block, block->InnerSize()),
            pw::Status::FailedPrecondition());
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockSmallerWithNextFree) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  block = block->Next();
  size_t next_inner_size = block->Next()->InnerSize();

  // Shrink by less than the minimum needed for a block. The extra should be
  // added to the subsequent block.
  size_t delta = BlockType::kBlockOverhead - BlockType::kAlignment;
  size_t new_inner_size = block->InnerSize() - delta;
  EXPECT_EQ(BlockType::Resize(block, new_inner_size), pw::OkStatus());
  EXPECT_EQ(block->InnerSize(), new_inner_size);

  BlockType* next = block->Next();
  EXPECT_FALSE(next->Used());
  EXPECT_EQ(next->InnerSize(), next_inner_size + delta);
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockLargerWithNextFree) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  size_t next_inner_size = block->Next()->InnerSize();

  // Grow by less than the minimum needed for a block. The extra should be
  // added to the subsequent block.
  size_t delta = BlockType::kBlockOverhead;
  size_t new_inner_size = block->InnerSize() + delta;
  EXPECT_EQ(BlockType::Resize(block, new_inner_size), pw::OkStatus());
  EXPECT_EQ(block->InnerSize(), new_inner_size);

  BlockType* next = block->Next();
  EXPECT_FALSE(next->Used());
  EXPECT_EQ(next->InnerSize(), next_inner_size - delta);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeBlockMuchLargerWithNextFree) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  size_t new_inner_size = block->InnerSize() + kOuterSize + 1;
  EXPECT_EQ(BlockType::Resize(block, new_inner_size), pw::Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockSmallerWithNextUsed) {
  constexpr Layout kLayout(256, kAlign);
  constexpr size_t kOuterSize = BlockType::kBlockOverhead + kLayout.size();

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Shrink the block.
  size_t delta = kLayout.size() / 2;
  size_t new_inner_size = block->InnerSize() - delta;
  EXPECT_EQ(BlockType::Resize(block, new_inner_size), pw::OkStatus());
  EXPECT_EQ(block->InnerSize(), new_inner_size);

  BlockType* next = block->Next();
  EXPECT_FALSE(next->Used());
  EXPECT_EQ(next->OuterSize(), delta);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeBlockLargerWithNextUsed) {
  constexpr size_t kOuterSize = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  size_t delta = BlockType::kBlockOverhead / 2;
  size_t new_inner_size = block->InnerSize() + delta;
  EXPECT_EQ(BlockType::Resize(block, new_inner_size), pw::Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeFromNull) {
  BlockType* block = nullptr;
  EXPECT_EQ(BlockType::Resize(block, 1), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CanCheckValidBlock) {
  constexpr size_t kOuterSize1 = 512;
  constexpr size_t kOuterSize2 = 256;

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize1, Preallocation::kUsed},
          {kOuterSize2, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  EXPECT_TRUE(block->IsValid());
  block->CrashIfInvalid();

  block = block->Next();
  EXPECT_TRUE(block->IsValid());
  block->CrashIfInvalid();

  block = block->Next();
  EXPECT_TRUE(block->IsValid());
  block->CrashIfInvalid();
}

TEST_FOR_EACH_BLOCK_TYPE(CanCheckInvalidBlock) {
  constexpr size_t kOuterSize1 = 128;
  constexpr size_t kOuterSize2 = 384;

  auto* block1 = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize1, Preallocation::kUsed},
          {kOuterSize2, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  BlockType* block2 = block1->Next();
  BlockType* block3 = block2->Next();

  // Corrupt a Block header.
  // This must not touch memory outside the original region, or the test may
  // (correctly) abort when run with address sanitizer.
  // To remain as agostic to the internals of `Block` as possible, the test
  // copies a smaller block's header to a larger block.
  EXPECT_TRUE(block1->IsValid());
  EXPECT_TRUE(block2->IsValid());
  EXPECT_TRUE(block3->IsValid());
  auto* src = reinterpret_cast<std::byte*>(block1);
  auto* dst = reinterpret_cast<std::byte*>(block2);
  std::memcpy(dst, src, sizeof(BlockType));
  EXPECT_FALSE(block1->IsValid());
  EXPECT_FALSE(block2->IsValid());
  EXPECT_FALSE(block3->IsValid());
}

TEST_F(PoisonedBlockTest, CanCheckPoison) {
  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  // Modify a byte in the middle of a free block.
  // Without poisoning, the modification is undetected.
  EXPECT_FALSE(block->Used());
  bytes_[kN / 2] = std::byte(0x7f);
  EXPECT_TRUE(block->IsValid());

  // Modify a byte in the middle of a free block.
  // With poisoning, the modification is detected.
  block->Poison();
  bytes_[kN / 2] = std::byte(0x7f);
  EXPECT_FALSE(block->IsValid());
}

TEST_FOR_EACH_BLOCK_TYPE(CanGetBlockFromUsableSpace) {
  auto* block1 = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  void* ptr = block1->UsableSpace();
  BlockType* block2 = BlockType::FromUsableSpace(ptr);
  EXPECT_EQ(block1, block2);
}

TEST_FOR_EACH_BLOCK_TYPE(CanGetConstBlockFromUsableSpace) {
  const auto* block1 = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  const void* ptr = block1->UsableSpace();
  const BlockType* block2 = BlockType::FromUsableSpace(ptr);
  EXPECT_EQ(block1, block2);
}

TEST_FOR_EACH_BLOCK_TYPE(CanGetAlignmentFromUsedBlock) {
  constexpr Layout kLayout1(128, kAlign);
  constexpr Layout kLayout2(384, kAlign * 2);

  auto* block1 = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  auto result = BlockType::AllocFirst(block1, kLayout1);
  ASSERT_EQ(result.status(), pw::OkStatus());

  BlockType* block2 = block1->Next();
  result = BlockType::AllocFirst(block2, kLayout2);
  ASSERT_EQ(result.status(), pw::OkStatus());

  EXPECT_EQ(block1->Alignment(), kAlign);
  EXPECT_EQ(block2->Alignment(), kAlign * 2);
}

TEST_FOR_EACH_BLOCK_TYPE(FreeBlockAlignmentIsAlwaysOne) {
  constexpr Layout kLayout1(128, kAlign);
  constexpr Layout kLayout2(384, kAlign * 2);

  auto* block1 = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  auto result = BlockType::AllocFirst(block1, kLayout1);
  ASSERT_EQ(result.status(), pw::OkStatus());

  BlockType* block2 = block1->Next();
  result = BlockType::AllocFirst(block2, kLayout2);
  ASSERT_EQ(result.status(), pw::OkStatus());

  EXPECT_EQ(block1->Alignment(), kAlign);
  BlockType::Free(block1);
  EXPECT_EQ(block1->Alignment(), 1U);
}

}  // namespace
