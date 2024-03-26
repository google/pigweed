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

#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {

using LargeOffsetBlock = Block<uint64_t>;
using SmallOffsetBlock = Block<uint16_t>;
using PoisonedBlock = Block<uint32_t, alignof(uint32_t), true>;

// Macro to provide type-parameterized tests for the various block types above.
//
// Ideally, the unit tests below could just use `TYPED_TEST_P` and its
// asscoiated macros from GoogleTest, see
// https://github.com/google/googletest/blob/main/docs/advanced.md#type-parameterized-tests
//
// These macros are not supported by the light framework however, so this macro
// provides a custom implementation that works just for these types.
#define TEST_FOR_EACH_BLOCK_TYPE(TestCase)                               \
  template <typename BlockType>                                          \
  void TestCase();                                                       \
  TEST(LargeOffsetBlockTest, TestCase) { TestCase<LargeOffsetBlock>(); } \
  TEST(SmallOffsetBlockTest, TestCase) { TestCase<SmallOffsetBlock>(); } \
  TEST(PoisonedBlockTest, TestCase) { TestCase<PoisonedBlock>(); }       \
  template <typename BlockType>                                          \
  void TestCase()

TEST_FOR_EACH_BLOCK_TYPE(CanCreateSingleAlignedBlock) {
  constexpr size_t kN = 1024;
  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;

  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(block->OuterSize(), kN);
  EXPECT_EQ(block->InnerSize(), kN - BlockType::kBlockOverhead);
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_EQ(block->Next(), nullptr);
  EXPECT_FALSE(block->Used());
  EXPECT_TRUE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanCreateUnalignedSingleBlock) {
  constexpr size_t kN = 1024;

  // Force alignment, so we can un-force it below
  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  ByteSpan aligned(bytes);

  auto result = BlockType::Init(aligned.subspan(1));
  EXPECT_EQ(result.status(), OkStatus());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotCreateTooSmallBlock) {
  std::array<std::byte, 2> bytes;
  auto result = BlockType::Init(span(bytes));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::ResourceExhausted());
}

TEST(SmallOffsetBlockTest, CannotCreateTooLargeBlock) {
  constexpr size_t kN = 1024;

  alignas(Block<uint8_t>::kAlignment) std::array<std::byte, kN> bytes;
  Result<Block<uint8_t>*> result = Block<uint8_t>::Init(span(bytes));
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CanSplitBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  auto* block1 = *result;

  result = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());

  auto* block2 = *result;

  EXPECT_EQ(block1->InnerSize(), kSplitN);
  EXPECT_EQ(block1->OuterSize(), kSplitN + BlockType::kBlockOverhead);
  EXPECT_FALSE(block1->Last());

  EXPECT_EQ(block2->OuterSize(), kN - kSplitN - BlockType::kBlockOverhead);
  EXPECT_FALSE(block2->Used());
  EXPECT_TRUE(block2->Last());

  EXPECT_EQ(block1->Next(), block2);
  EXPECT_EQ(block2->Prev(), block1);
}

TEST_FOR_EACH_BLOCK_TYPE(CanSplitBlockUnaligned) {
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  // We should split at sizeof(BlockType) + kSplitN bytes. Then
  // we need to round that up to an alignof(BlockType) boundary.
  constexpr size_t kSplitN = 513;
  uintptr_t split_addr = reinterpret_cast<uintptr_t>(block1) + kSplitN;
  split_addr += alignof(BlockType) - (split_addr % alignof(BlockType));
  uintptr_t split_len = split_addr - (uintptr_t)&bytes;

  result = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(block1->InnerSize(), split_len);
  EXPECT_EQ(block1->OuterSize(), split_len + BlockType::kBlockOverhead);

  EXPECT_EQ(block2->OuterSize(), kN - block1->OuterSize());
  EXPECT_FALSE(block2->Used());

  EXPECT_EQ(block1->Next(), block2);
  EXPECT_EQ(block2->Prev(), block1);
}

TEST_FOR_EACH_BLOCK_TYPE(CanSplitMidBlock) {
  // Split once, then split the original block again to ensure that the
  // pointers get rewired properly.
  // I.e.
  // [[             BLOCK 1            ]]
  // block1->Split()
  // [[       BLOCK1       ]][[ BLOCK2 ]]
  // block1->Split()
  // [[ BLOCK1 ]][[ BLOCK3 ]][[ BLOCK2 ]]

  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block1, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block3 = *result;

  EXPECT_EQ(block1->Next(), block3);
  EXPECT_EQ(block3->Prev(), block1);
  EXPECT_EQ(block3->Next(), block2);
  EXPECT_EQ(block2->Prev(), block3);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotSplitTooSmallBlock) {
  constexpr size_t kN = 64;
  constexpr size_t kSplitN = kN + 1;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, kSplitN);
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotSplitBlockWithoutHeaderSpace) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = kN - BlockType::kBlockOverhead - 1;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, kSplitN);
  EXPECT_EQ(result.status(), Status::ResourceExhausted());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotSplitNull) {
  BlockType* block = nullptr;
  auto result = BlockType::Split(block, 1);
  EXPECT_EQ(result.status(), Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotMakeBlockLargerInSplit) {
  // Ensure that we can't ask for more space than the block actually has...
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, block->InnerSize() + 1);
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotMakeSecondBlockLargerInSplit) {
  // Ensure that the second block in split is at least of the size of header.
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block,
                            block->InnerSize() - BlockType::kBlockOverhead + 1);
  EXPECT_EQ(result.status(), Status::ResourceExhausted());
}

TEST_FOR_EACH_BLOCK_TYPE(CanMakeZeroSizeFirstBlock) {
  // This block does support splitting with zero payload size.
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, 0);
  ASSERT_EQ(result.status(), OkStatus());
  EXPECT_EQ(block->InnerSize(), static_cast<size_t>(0));
}

TEST_FOR_EACH_BLOCK_TYPE(CanMakeZeroSizeSecondBlock) {
  // Likewise, the split block can be zero-width.
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result =
      BlockType::Split(block1, block1->InnerSize() - BlockType::kBlockOverhead);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(block2->InnerSize(), static_cast<size_t>(0));
}

TEST_FOR_EACH_BLOCK_TYPE(CanMarkBlockUsed) {
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  EXPECT_TRUE(block->Used());

  // Size should be unaffected.
  EXPECT_EQ(block->OuterSize(), kN);

  block->MarkFree();
  EXPECT_FALSE(block->Used());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotSplitUsedBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  result = BlockType::Split(block, kSplitN);
  EXPECT_EQ(result.status(), Status::FailedPrecondition());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirstFromAlignedBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSize = 256;
  constexpr size_t kAlign = 32;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Make sure the block's usable space is aligned.
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_outer_size = AlignUp(addr, kAlign) - addr;
  if (pad_outer_size != 0) {
    while (pad_outer_size < BlockType::kBlockOverhead) {
      pad_outer_size += kAlign;
    }
    result =
        BlockType::Split(block, pad_outer_size - BlockType::kBlockOverhead);
    EXPECT_EQ(result.status(), OkStatus());
    block = *result;
  }

  // Allocate from the front of the block.
  BlockType* prev = block->Prev();
  EXPECT_EQ(BlockType::AllocFirst(block, kSize, kAlign), OkStatus());
  EXPECT_EQ(block->InnerSize(), kSize);
  addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  // No new padding block was allocated.
  EXPECT_EQ(prev, block->Prev());

  // Extra was split from the end of the block.
  EXPECT_FALSE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirstFromUnalignedBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSize = 256;
  constexpr size_t kAlign = 32;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Make sure the block's usable space is not aligned.
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_inner_size = AlignUp(addr, kAlign) - addr + (kAlign / 2);
  if (pad_inner_size < BlockType::kBlockOverhead) {
    pad_inner_size += kAlign;
  }
  pad_inner_size -= BlockType::kBlockOverhead;
  result = BlockType::Split(block, pad_inner_size);
  EXPECT_EQ(result.status(), OkStatus());
  block = *result;

  // Allocate from the front of the block.
  BlockType* prev = block->Prev();
  EXPECT_EQ(BlockType::AllocFirst(block, kSize, kAlign), OkStatus());
  EXPECT_EQ(block->InnerSize(), kSize);
  addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  // A new padding block was allocated.
  EXPECT_LT(prev, block->Prev());

  // Extra was split from the end of the block.
  EXPECT_FALSE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirstTooSmallBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kAlign = 32;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Make sure the block's usable space is not aligned.
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_inner_size = AlignUp(addr, kAlign) - addr + (kAlign / 2);
  if (pad_inner_size < BlockType::kBlockOverhead) {
    pad_inner_size += kAlign;
  }
  pad_inner_size -= BlockType::kBlockOverhead;
  result = BlockType::Split(block, pad_inner_size);
  EXPECT_EQ(result.status(), OkStatus());
  block = *result;

  // Cannot allocate without room to a split a block for alignment.
  EXPECT_EQ(BlockType::AllocFirst(block, block->InnerSize(), kAlign),
            Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirstFromNull) {
  BlockType* block = nullptr;
  EXPECT_EQ(BlockType::AllocFirst(block, 1, 1), Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLast) {
  constexpr size_t kN = 1024;
  constexpr size_t kSize = 256;
  constexpr size_t kAlign = 32;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Allocate from the back of the block.
  EXPECT_EQ(BlockType::AllocLast(block, kSize, kAlign), OkStatus());
  EXPECT_GE(block->InnerSize(), kSize);
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());

  // Extra was split from the front of the block.
  EXPECT_FALSE(block->Prev()->Used());
  EXPECT_TRUE(block->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLastFromTooSmallBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kAlign = 32;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Make sure the block's usable space is not aligned.
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_inner_size = AlignUp(addr, kAlign) - addr + (kAlign / 2);
  if (pad_inner_size < BlockType::kBlockOverhead) {
    pad_inner_size += kAlign;
  }
  pad_inner_size -= BlockType::kBlockOverhead;
  result = BlockType::Split(block, pad_inner_size);
  EXPECT_EQ(result.status(), OkStatus());
  block = *result;

  // Cannot allocate without room to a split a block for alignment.
  EXPECT_EQ(BlockType::AllocLast(block, block->InnerSize(), kAlign),
            Status::ResourceExhausted());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLastFromNull) {
  BlockType* block = nullptr;
  EXPECT_EQ(BlockType::AllocLast(block, 1, 1), Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CanMergeWithNextBlock) {
  // Do the three way merge from "CanSplitMidBlock", and let's
  // merge block 3 and 2
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());

  result = BlockType::Split(block1, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block3 = *result;

  EXPECT_EQ(BlockType::MergeNext(block3), OkStatus());

  EXPECT_EQ(block1->Next(), block3);
  EXPECT_EQ(block3->Prev(), block1);
  EXPECT_EQ(block1->InnerSize(), kSplit2);
  EXPECT_EQ(block3->OuterSize(), kN - block1->OuterSize());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotMergeWithFirstOrLastBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  // Do a split, just to check that the checks on Next/Prev are different...
  result = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(BlockType::MergeNext(block2), Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotMergeNull) {
  BlockType* block = nullptr;
  EXPECT_EQ(BlockType::MergeNext(block), Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotMergeUsedBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Do a split, just to check that the checks on Next/Prev are different...
  result = BlockType::Split(block, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());

  block->MarkUsed();
  EXPECT_EQ(BlockType::MergeNext(block), Status::FailedPrecondition());
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeSingleBlock) {
  constexpr size_t kN = 1024;
  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;

  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(block->OuterSize(), kN);
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockWithoutMerging) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block3 = *result;

  block1->MarkUsed();
  block2->MarkUsed();
  block3->MarkUsed();

  BlockType::Free(block2);
  EXPECT_FALSE(block2->Used());
  EXPECT_NE(block2->Prev(), nullptr);
  EXPECT_FALSE(block2->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithPrev) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block3 = *result;

  block2->MarkUsed();
  block3->MarkUsed();

  BlockType::Free(block2);
  EXPECT_FALSE(block2->Used());
  EXPECT_EQ(block2->Prev(), nullptr);
  EXPECT_FALSE(block2->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithNext) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());

  block1->MarkUsed();
  block2->MarkUsed();

  BlockType::Free(block2);
  EXPECT_FALSE(block2->Used());
  EXPECT_NE(block2->Prev(), nullptr);
  EXPECT_TRUE(block2->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeUsedBlockAndMergeWithBoth) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());

  block2->MarkUsed();

  BlockType::Free(block2);
  EXPECT_FALSE(block2->Used());
  EXPECT_EQ(block2->Prev(), nullptr);
  EXPECT_TRUE(block2->Last());
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockSameSize) {
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  EXPECT_EQ(BlockType::Resize(block, block->InnerSize()), OkStatus());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeFreeBlock) {
  constexpr size_t kN = 1024;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(BlockType::Resize(block, block->InnerSize()),
            Status::FailedPrecondition());
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockSmallerWithNextFree) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  block1->MarkUsed();
  size_t block2_inner_size = block2->InnerSize();

  // Shrink by less than the minimum needed for a block. The extra should be
  // added to the subsequent block.
  size_t delta = BlockType::kBlockOverhead - BlockType::kAlignment;
  size_t new_inner_size = block1->InnerSize() - delta;
  EXPECT_EQ(BlockType::Resize(block1, new_inner_size), OkStatus());
  EXPECT_EQ(block1->InnerSize(), new_inner_size);

  block2 = block1->Next();
  EXPECT_GE(block2->InnerSize(), block2_inner_size + delta);
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockLargerWithNextFree) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  block1->MarkUsed();
  size_t block2_inner_size = block2->InnerSize();

  // Grow by less than the minimum needed for a block. The extra should be
  // added to the subsequent block.
  size_t delta = BlockType::kBlockOverhead - BlockType::kAlignment;
  size_t new_inner_size = block1->InnerSize() + delta;
  EXPECT_EQ(BlockType::Resize(block1, new_inner_size), OkStatus());
  EXPECT_EQ(block1->InnerSize(), new_inner_size);

  block2 = block1->Next();
  EXPECT_GE(block2->InnerSize(), block2_inner_size - delta);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeBlockMuchLargerWithNextFree) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block3 = *result;

  block1->MarkUsed();
  block3->MarkUsed();

  size_t new_inner_size = block1->InnerSize() + block2->OuterSize() + 1;
  EXPECT_EQ(BlockType::Resize(block1, new_inner_size), Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CanResizeBlockSmallerWithNextUsed) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  block1->MarkUsed();
  block2->MarkUsed();

  // Shrink the block.
  size_t delta = kSplit1 / 2;
  size_t new_inner_size = block1->InnerSize() - delta;
  EXPECT_EQ(BlockType::Resize(block1, new_inner_size), OkStatus());
  EXPECT_EQ(block1->InnerSize(), new_inner_size);

  block2 = block1->Next();
  EXPECT_EQ(block2->OuterSize(), delta);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeBlockLargerWithNextUsed) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  block1->MarkUsed();
  block2->MarkUsed();

  size_t delta = BlockType::kBlockOverhead / 2;
  size_t new_inner_size = block1->InnerSize() + delta;
  EXPECT_EQ(BlockType::Resize(block1, new_inner_size), Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeFromTooNull) {
  BlockType* block = nullptr;
  EXPECT_EQ(BlockType::Resize(block, 1), Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CanCheckValidBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> bytes;
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block3 = *result;

  EXPECT_TRUE(block1->IsValid());
  block1->CrashIfInvalid();

  EXPECT_TRUE(block2->IsValid());
  block2->CrashIfInvalid();

  EXPECT_TRUE(block3->IsValid());
  block3->CrashIfInvalid();
}

TEST_FOR_EACH_BLOCK_TYPE(CanCheckInvalidBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 128;
  constexpr size_t kSplit2 = 384;
  constexpr size_t kSplit3 = 256;

  std::array<std::byte, kN> bytes{};
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  result = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block3 = *result;

  result = BlockType::Split(block3, kSplit3);
  ASSERT_EQ(result.status(), OkStatus());

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

TEST(PoisonedBlockTest, CanCheckPoison) {
  constexpr size_t kN = 1024;
  // constexpr size_t kSplit1 = 512;
  std::array<std::byte, kN> bytes{};
  auto result = PoisonedBlock::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  PoisonedBlock* block = *result;

  // Modify a byte in the middle of a free block.
  // Without poisoning, the modification is undetected.
  EXPECT_FALSE(block->Used());
  bytes[kN / 2] = std::byte(0x7f);
  EXPECT_TRUE(block->IsValid());

  // Modify a byte in the middle of a free block.
  // With poisoning, the modification is detected.
  block->Poison();
  bytes[kN / 2] = std::byte(0x7f);
  EXPECT_FALSE(block->IsValid());
}

TEST_FOR_EACH_BLOCK_TYPE(CanGetBlockFromUsableSpace) {
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes{};
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  void* ptr = block1->UsableSpace();
  BlockType* block2 = BlockType::FromUsableSpace(ptr);
  EXPECT_EQ(block1, block2);
}

TEST_FOR_EACH_BLOCK_TYPE(CanGetConstBlockFromUsableSpace) {
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes{};
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  const BlockType* block1 = *result;

  const void* ptr = block1->UsableSpace();
  const BlockType* block2 = BlockType::FromUsableSpace(ptr);
  EXPECT_EQ(block1, block2);
}

TEST_FOR_EACH_BLOCK_TYPE(CanGetAlignmentFromUsedBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 128;
  constexpr size_t kSplit2 = 512;
  constexpr size_t kAlign = 32;

  std::array<std::byte, kN> bytes{};
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  EXPECT_EQ(BlockType::AllocFirst(block1, kSplit1, kAlign), OkStatus());
  BlockType* block2 = block1->Next();
  EXPECT_EQ(BlockType::AllocFirst(block2, kSplit2, kAlign * 2), OkStatus());

  EXPECT_EQ(block1->Alignment(), kAlign);
  EXPECT_EQ(block2->Alignment(), kAlign * 2);
}

TEST_FOR_EACH_BLOCK_TYPE(FreeBlockAlignmentIsAlwaysOne) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 128;
  constexpr size_t kAlign = 32;

  std::array<std::byte, kN> bytes{};
  auto result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  EXPECT_EQ(BlockType::AllocFirst(block1, kSplit1, kAlign), OkStatus());
  block1->MarkFree();
  EXPECT_EQ(block1->Alignment(), 1U);
}

}  // namespace pw::allocator
