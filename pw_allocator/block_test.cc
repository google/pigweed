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

using std::byte;

namespace pw::allocator {

template <typename BlockType>
void CanCreateSingleAlignedBlock() {
  constexpr size_t kN = 1024;
  alignas(BlockType*) std::array<std::byte, kN> bytes;

  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(block->OuterSize(), kN);
  EXPECT_EQ(block->InnerSize(), kN - BlockType::kBlockOverhead);
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_EQ(block->Next(), nullptr);
  EXPECT_FALSE(block->Used());
  EXPECT_TRUE(block->Last());
}
TEST(GenericBlockTest, CanCreateSingleBlock) {
  CanCreateSingleAlignedBlock<Block<>>();
}
TEST(CustomBlockTest, CanCreateSingleBlock) {
  CanCreateSingleAlignedBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanCreateUnalignedSingleBlock() {
  constexpr size_t kN = 1024;

  // Force alignment, so we can un-force it below
  alignas(BlockType*) std::array<std::byte, kN> bytes;
  ByteSpan aligned(bytes);

  Result<BlockType*> result = BlockType::Init(aligned.subspan(1));
  EXPECT_EQ(result.status(), OkStatus());
}
TEST(GenericBlockTest, CannotCreateUnalignedSingleBlock) {
  CanCreateUnalignedSingleBlock<Block<>>();
}
TEST(CustomBlockTest, CannotCreateUnalignedSingleBlock) {
  CanCreateUnalignedSingleBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotCreateTooSmallBlock() {
  std::array<std::byte, 2> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::ResourceExhausted());
}
TEST(GenericBlockTest, CannotCreateTooSmallBlock) {
  CannotCreateTooSmallBlock<Block<>>();
}
TEST(CustomBlockTest, CannotCreateTooSmallBlock) {
  CannotCreateTooSmallBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

TEST(CustomBlockTest, CannotCreateTooLargeBlock) {
  constexpr size_t kN = 1024;
  using BlockType = Block<uint8_t>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

template <typename BlockType>
void CanSplitBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(block1->InnerSize(), kSplitN);
  EXPECT_EQ(block1->OuterSize(), kSplitN + BlockType::kBlockOverhead);
  EXPECT_FALSE(block1->Last());

  EXPECT_EQ(block2->OuterSize(), kN - kSplitN - BlockType::kBlockOverhead);
  EXPECT_FALSE(block2->Used());
  EXPECT_TRUE(block2->Last());

  EXPECT_EQ(block1->Next(), block2);
  EXPECT_EQ(block2->Prev(), block1);
}
TEST(GenericBlockTest, CanSplitBlock) { CanSplitBlock<Block<>>(); }
TEST(CustomBlockTest, CanSplitBlock) {
  CanSplitBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanSplitBlockUnaligned() {
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  // We should split at sizeof(BlockType) + kSplitN bytes. Then
  // we need to round that up to an alignof(BlockType*) boundary.
  constexpr size_t kSplitN = 513;
  uintptr_t split_addr = reinterpret_cast<uintptr_t>(block1) + kSplitN;
  split_addr += alignof(BlockType*) - (split_addr % alignof(BlockType*));
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
TEST(GenericBlockTest, CanSplitBlockUnaligned) { CanSplitBlock<Block<>>(); }
TEST(CustomBlockTest, CanSplitBlockUnaligned) {
  CanSplitBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanSplitMidBlock() {
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

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanSplitMidBlock) { CanSplitMidBlock<Block<>>(); }
TEST(CustomBlockTest, CanSplitMidBlock) {
  CanSplitMidBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotSplitTooSmallBlock() {
  constexpr size_t kN = 64;
  constexpr size_t kSplitN = kN + 1;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, kSplitN);
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

template <typename BlockType>
void CannotSplitBlockWithoutHeaderSpace() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = kN - BlockType::kBlockOverhead - 1;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, kSplitN);
  EXPECT_EQ(result.status(), Status::ResourceExhausted());
}
TEST(GenericBlockTest, CannotSplitBlockWithoutHeaderSpace) {
  CannotSplitBlockWithoutHeaderSpace<Block<>>();
}
TEST(CustomBlockTest, CannotSplitBlockWithoutHeaderSpace) {
  CannotSplitBlockWithoutHeaderSpace<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotMakeBlockLargerInSplit() {
  // Ensure that we can't ask for more space than the block actually has...
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, block->InnerSize() + 1);
  EXPECT_EQ(result.status(), Status::OutOfRange());
}
TEST(GenericBlockTest, CannotMakeBlockLargerInSplit) {
  CannotMakeBlockLargerInSplit<Block<>>();
}
TEST(CustomBlockTest, CannotMakeBlockLargerInSplit) {
  CannotMakeBlockLargerInSplit<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotMakeSecondBlockLargerInSplit() {
  // Ensure that the second block in split is at least of the size of header.
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block,
                            block->InnerSize() - BlockType::kBlockOverhead + 1);
  EXPECT_EQ(result.status(), Status::ResourceExhausted());
}
TEST(GenericBlockTest, CannotMakeSecondBlockLargerInSplit) {
  CannotMakeSecondBlockLargerInSplit<Block<>>();
}
TEST(CustomBlockTest, CannotMakeSecondBlockLargerInSplit) {
  CannotMakeSecondBlockLargerInSplit<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanMakeZeroSizeFirstBlock() {
  // This block does support splitting with zero payload size.
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, 0);
  ASSERT_EQ(result.status(), OkStatus());
  EXPECT_EQ(block->InnerSize(), static_cast<size_t>(0));
}
TEST(GenericBlockTest, CanMakeZeroSizeFirstBlock) {
  CanMakeZeroSizeFirstBlock<Block<>>();
}
TEST(CustomBlockTest, CanMakeZeroSizeFirstBlock) {
  CanMakeZeroSizeFirstBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanMakeZeroSizeSecondBlock() {
  // Likewise, the split block can be zero-width.
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  result =
      BlockType::Split(block1, block1->InnerSize() - BlockType::kBlockOverhead);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(block2->InnerSize(), static_cast<size_t>(0));
}
TEST(GenericBlockTest, CanMakeZeroSizeSecondBlock) {
  CanMakeZeroSizeSecondBlock<Block<>>();
}
TEST(CustomBlockTest, CanMakeZeroSizeSecondBlock) {
  CanMakeZeroSizeSecondBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanMarkBlockUsed() {
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  EXPECT_TRUE(block->Used());

  // Size should be unaffected.
  EXPECT_EQ(block->OuterSize(), kN);

  block->MarkFree();
  EXPECT_FALSE(block->Used());
}
TEST(GenericBlockTest, CanMarkBlockUsed) { CanMarkBlockUsed<Block<>>(); }
TEST(CustomBlockTest, CanMarkBlockUsed) {
  CanMarkBlockUsed<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotSplitUsedBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  result = BlockType::Split(block, kSplitN);
  EXPECT_EQ(result.status(), Status::FailedPrecondition());
}
TEST(GenericBlockTest, CannotSplitUsedBlock) {
  CannotSplitUsedBlock<Block<>>();
}
TEST(CustomBlockTest, CannotSplitUsedBlock) {
  CannotSplitUsedBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanAllocFirstFromAlignedBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSize = 256;
  constexpr size_t kAlign = 32;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanAllocFirstFromAlignedBlock) {
  CanAllocFirstFromAlignedBlock<Block<>>();
}
TEST(CustomBlockTest, CanAllocFirstFromAlignedBlock) {
  CanAllocFirstFromAlignedBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanAllocFirstFromUnalignedBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSize = 256;
  constexpr size_t kAlign = 32;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Make sure the block's usable space is not aligned.
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_inner_size = AlignUp(addr, kAlign) - addr + (kAlign / 2);
  if (pad_inner_size < BlockType::kHeaderSize) {
    pad_inner_size += kAlign;
  }
  pad_inner_size -= BlockType::kHeaderSize;
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
TEST(GenericBlockTest, CanAllocFirstFromUnalignedBlock) {
  CanAllocFirstFromUnalignedBlock<Block<>>();
}
TEST(CustomBlockTest, CanAllocFirstFromUnalignedBlock) {
  CanAllocFirstFromUnalignedBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotAllocFirstTooSmallBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kAlign = 32;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Make sure the block's usable space is not aligned.
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_inner_size = AlignUp(addr, kAlign) - addr + (kAlign / 2);
  if (pad_inner_size < BlockType::kHeaderSize) {
    pad_inner_size += kAlign;
  }
  pad_inner_size -= BlockType::kHeaderSize;
  result = BlockType::Split(block, pad_inner_size);
  EXPECT_EQ(result.status(), OkStatus());
  block = *result;

  // Cannot allocate without room to a split a block for alignment.
  EXPECT_EQ(BlockType::AllocFirst(block, block->InnerSize(), kAlign),
            Status::OutOfRange());
}
TEST(GenericBlockTest, CannotAllocFirstTooSmallBlock) {
  CannotAllocFirstTooSmallBlock<Block<>>();
}
TEST(CustomBlockTest, CannotAllocFirstTooSmallBlock) {
  CannotAllocFirstTooSmallBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanAllocLast() {
  constexpr size_t kN = 1024;
  constexpr size_t kSize = 256;
  constexpr size_t kAlign = 32;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanAllocLast) { CanAllocLast<Block<>>(); }
TEST(CustomBlockTest, CanAllocLast) {
  CanAllocLast<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotAllocLastFromTooSmallBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kAlign = 32;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Make sure the block's usable space is not aligned.
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  size_t pad_inner_size = AlignUp(addr, kAlign) - addr + (kAlign / 2);
  if (pad_inner_size < BlockType::kHeaderSize) {
    pad_inner_size += kAlign;
  }
  pad_inner_size -= BlockType::kHeaderSize;
  result = BlockType::Split(block, pad_inner_size);
  EXPECT_EQ(result.status(), OkStatus());
  block = *result;

  // Cannot allocate without room to a split a block for alignment.
  EXPECT_EQ(BlockType::AllocLast(block, block->InnerSize(), kAlign),
            Status::ResourceExhausted());
}

TEST(GenericBlockTest, CannotAllocLastFromTooSmallBlock) {
  CannotAllocLastFromTooSmallBlock<Block<>>();
}
TEST(CustomBlockTest, CannotAllocLastFromTooSmallBlock) {
  CannotAllocLastFromTooSmallBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanMergeWithNextBlock() {
  // Do the three way merge from "CanSplitMidBlock", and let's
  // merge block 3 and 2
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanMergeWithNextBlock) {
  CanMergeWithNextBlock<Block<>>();
}
TEST(CustomBlockTest, CanMergeWithNextBlock) {
  CanMergeWithNextBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotMergeWithFirstOrLastBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;

  // Do a split, just to check that the checks on Next/Prev are different...
  result = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(BlockType::MergeNext(block2), Status::OutOfRange());

  BlockType* block0 = block1->Prev();
  EXPECT_EQ(BlockType::MergeNext(block0), Status::OutOfRange());
}
TEST(GenericBlockTest, CannotMergeWithFirstOrLastBlock) {
  CannotMergeWithFirstOrLastBlock<Block<>>();
}
TEST(CustomBlockTest, CannotMergeWithFirstOrLastBlock) {
  CannotMergeWithFirstOrLastBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotMergeUsedBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // Do a split, just to check that the checks on Next/Prev are different...
  result = BlockType::Split(block, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());

  block->MarkUsed();
  EXPECT_EQ(BlockType::MergeNext(block), Status::FailedPrecondition());
}
TEST(GenericBlockTest, CannotMergeUsedBlock) {
  CannotMergeUsedBlock<Block<>>();
}
TEST(CustomBlockTest, CannotMergeUsedBlock) {
  CannotMergeUsedBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanFreeSingleBlock() {
  constexpr size_t kN = 1024;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(block->OuterSize(), kN);
}
TEST(GenericBlockTest, CanFreeSingleBlock) { CanFreeSingleBlock<Block<>>(); }
TEST(CustomBlockTest, CanFreeSingleBlock) {
  CanFreeSingleBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanFreeBlockWithoutMerging() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanFreeBlockWithoutMerging) {
  CanFreeBlockWithoutMerging<Block<>>();
}
TEST(CustomBlockTest, CanFreeBlockWithoutMerging) {
  CanFreeBlockWithoutMerging<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanFreeBlockAndMergeWithPrev() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanFreeBlockAndMergeWithPrev) {
  CanFreeBlockAndMergeWithPrev<Block<>>();
}
TEST(CustomBlockTest, CanFreeBlockAndMergeWithPrev) {
  CanFreeBlockAndMergeWithPrev<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanFreeBlockAndMergeWithNext() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanFreeBlockAndMergeWithNext) {
  CanFreeBlockAndMergeWithNext<Block<>>();
}
TEST(CustomBlockTest, CanFreeBlockAndMergeWithNext) {
  CanFreeBlockAndMergeWithNext<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanFreeUsedBlockAndMergeWithBoth() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanFreeUsedBlockAndMergeWithBoth) {
  CanFreeUsedBlockAndMergeWithBoth<Block<>>();
}
TEST(CustomBlockTest, CanFreeUsedBlockAndMergeWithBoth) {
  CanFreeUsedBlockAndMergeWithBoth<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanResizeBlockSameSize() {
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  EXPECT_EQ(BlockType::Resize(block, block->InnerSize()), OkStatus());
}
TEST(GenericBlockTest, CanResizeBlockSameSize) {
  CanResizeBlockSameSize<Block<>>();
}
TEST(CustomBlockTest, CanResizeBlockSameSize) {
  CanResizeBlockSameSize<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotResizeFreeBlock() {
  constexpr size_t kN = 1024;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(BlockType::Resize(block, block->InnerSize()),
            Status::FailedPrecondition());
}
TEST(GenericBlockTest, CannotResizeFreeBlock) {
  CannotResizeFreeBlock<Block<>>();
}
TEST(CustomBlockTest, CannotResizeFreeBlock) {
  CannotResizeFreeBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanResizeBlockSmallerWithNextFree() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanResizeBlockSmallerWithNextFree) {
  CanResizeBlockSmallerWithNextFree<Block<>>();
}
TEST(CustomBlockTest, CanResizeBlockSmallerWithNextFree) {
  CanResizeBlockSmallerWithNextFree<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanResizeBlockLargerWithNextFree() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanResizeBlockLargerWithNextFree) {
  CanResizeBlockLargerWithNextFree<Block<>>();
}
TEST(CustomBlockTest, CanResizeBlockLargerWithNextFree) {
  CanResizeBlockLargerWithNextFree<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotResizeBlockMuchLargerWithNextFree() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CannotResizeBlockMuchLargerWithNextFree) {
  CannotResizeBlockMuchLargerWithNextFree<Block<>>();
}
TEST(CustomBlockTest, CannotResizeBlockMuchLargerWithNextFree) {
  CannotResizeBlockMuchLargerWithNextFree<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanResizeBlockSmallerWithNextUsed() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanResizeBlockSmallerWithNextUsed) {
  CanResizeBlockSmallerWithNextUsed<Block<>>();
}
TEST(CustomBlockTest, CanResizeBlockSmallerWithNextUsed) {
  CanResizeBlockSmallerWithNextUsed<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CannotResizeBlockLargerWithNextUsed() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CannotResizeBlockLargerWithNextUsed) {
  CannotResizeBlockLargerWithNextUsed<Block<>>();
}
TEST(CustomBlockTest, CannotResizeBlockLargerWithNextUsed) {
  CannotResizeBlockLargerWithNextUsed<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanCheckValidBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
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
TEST(GenericBlockTest, CanCheckValidBlock) { CanCheckValidBlock<Block<>>(); }
TEST(CustomBlockTest, CanCheckValidBlock) {
  CanCheckValidBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

template <typename BlockType>
void CanCheckInvalidBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 128;
  constexpr size_t kSplit3 = 256;

  std::array<std::byte, kN> bytes{};
  Result<BlockType*> result = BlockType::Init(span(bytes));
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

#if defined(PW_ALLOCATOR_POISON_ENABLE) && PW_ALLOCATOR_POISON_ENABLE
  // Corrupt a byte in the poisoned header.
  EXPECT_TRUE(block1->IsValid());
  bytes[BlockType::kHeaderSize - 1] = std::byte(0xFF);
  EXPECT_FALSE(block1->IsValid());

  // Corrupt a byte in the poisoned footer.
  EXPECT_TRUE(block2->IsValid());
  bytes[block1->OuterSize() + block2->OuterSize() - 1] = std::byte(0xFF);
  EXPECT_FALSE(block2->IsValid());
#endif  // PW_ALLOCATOR_POISON_ENABLE

  // Corrupt a Block header.
  // This must not touch memory outside the original region, or the test may
  // (correctly) abort when run with address sanitizer.
  // To remain as agostic to the internals of `Block` as possible, the test
  // copies a smaller block's header to a larger block.
  EXPECT_TRUE(block3->IsValid());
  auto* src = reinterpret_cast<std::byte*>(block2);
  auto* dst = reinterpret_cast<std::byte*>(block3);
  std::memcpy(dst, src, sizeof(BlockType));
  EXPECT_FALSE(block3->IsValid());
}
TEST(GenericBlockTest, CanCheckInvalidBlock) {
  CanCheckInvalidBlock<Block<>>();
}
TEST(CustomBlockTest, CanCheckInvalidBlock) {
  CanCheckInvalidBlock<Block<uint32_t, sizeof(uint16_t)>>();
}

TEST(CustomBlockTest, NoFlagsbyDefault) {
  constexpr size_t kN = 1024;
  using BlockType = Block<>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetFlags(std::numeric_limits<BlockType::offset_type>::max());
  EXPECT_EQ(block->GetFlags(), 0U);
}

TEST(CustomBlockTest, CustomFlagsInitiallyZero) {
  constexpr size_t kN = 1024;
  constexpr size_t kNumFlags = 10;
  using BlockType = Block<uint16_t, 0, kNumFlags>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(block->GetFlags(), 0U);
}

TEST(CustomBlockTest, SetCustomFlags) {
  constexpr size_t kN = 1024;
  constexpr size_t kNumFlags = 10;
  using BlockType = Block<uint16_t, 0, kNumFlags>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetFlags(1);
  EXPECT_EQ(block->GetFlags(), 1U);
}

TEST(CustomBlockTest, SetAllCustomFlags) {
  constexpr size_t kN = 1024;
  constexpr size_t kNumFlags = 10;
  using BlockType = Block<uint16_t, 0, kNumFlags>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  // `1024/alignof(uint16_t)` is `0x200`, which leaves 6 bits available for
  // flags per offset field. After 1 builtin field, this leaves 2*5 available
  // for custom flags.
  block->SetFlags((uint16_t(1) << 10) - 1);
  EXPECT_EQ(block->GetFlags(), 0x3FFU);
}

TEST(CustomBlockTest, ClearCustomFlags) {
  constexpr size_t kN = 1024;
  constexpr size_t kNumFlags = 10;
  using BlockType = Block<uint16_t, 0, kNumFlags>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetFlags(0x155);
  block->SetFlags(0x2AA, 0x333);
  EXPECT_EQ(block->GetFlags(), 0x2EEU);
}

TEST(CustomBlockTest, FlagsNotCopiedOnSplit) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  constexpr size_t kNumFlags = 10;
  using BlockType = Block<uint16_t, 0, kNumFlags>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;
  block1->SetFlags(0x137);

  result = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(block1->GetFlags(), 0x137U);
  EXPECT_EQ(block2->GetFlags(), 0U);
}

TEST(CustomBlockTest, FlagsPreservedByMergeNext) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  constexpr size_t kNumFlags = 10;
  using BlockType = Block<uint16_t, 0, kNumFlags>;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());

  block->SetFlags(0x137);
  EXPECT_EQ(BlockType::MergeNext(block), OkStatus());
  EXPECT_EQ(block->GetFlags(), 0x137U);
}

TEST(GenericBlockTest, SetAndGetExtraBytes) {
  constexpr size_t kN = 1024;
  using BlockType = Block<>;
  constexpr size_t kExtraN = 4;
  constexpr std::array<uint8_t, kExtraN> kExtra{0xa1, 0xb2, 0xc3, 0xd4};

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetExtraBytes(as_bytes(span(kExtra)));
  ConstByteSpan extra = block->GetExtraBytes();
  EXPECT_EQ(extra.size(), 0U);
}

TEST(CustomBlockTest, SetAndGetExtraBytes) {
  constexpr size_t kN = 1024;
  constexpr size_t kNumExtraBytes = 4;
  using BlockType = Block<uintptr_t, kNumExtraBytes>;
  constexpr size_t kExtraN = 4;
  constexpr std::array<uint8_t, kExtraN> kExtra{0xa1, 0xb2, 0xc3, 0xd4};

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetExtraBytes(as_bytes(span(kExtra)));
  ConstByteSpan extra = block->GetExtraBytes();
  EXPECT_EQ(extra.size(), kNumExtraBytes);
  EXPECT_EQ(std::memcmp(extra.data(), kExtra.data(), kExtraN), 0);
}

TEST(CustomBlockTest, SetExtraBytesPadsWhenShort) {
  constexpr size_t kN = 1024;
  constexpr size_t kNumExtraBytes = 8;
  using BlockType = Block<uintptr_t, kNumExtraBytes>;
  constexpr size_t kExtraN = 4;
  constexpr std::array<uint8_t, kExtraN> kExtra{0xa1, 0xb2, 0xc3, 0xd4};

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetExtraBytes(as_bytes(span(kExtra)));
  ConstByteSpan extra = block->GetExtraBytes();
  EXPECT_EQ(extra.size(), kNumExtraBytes);
  EXPECT_EQ(std::memcmp(extra.data(), kExtra.data(), kExtraN), 0);
  for (size_t i = kExtraN; i < kNumExtraBytes; ++i) {
    EXPECT_EQ(size_t(extra[i]), 0U);
  }
}

TEST(CustomBlockTest, SetExtraBytesTruncatesWhenLong) {
  constexpr size_t kN = 1024;
  constexpr size_t kNumExtraBytes = 2;
  using BlockType = Block<uintptr_t, kNumExtraBytes>;
  constexpr size_t kExtraN = 4;
  constexpr std::array<uint8_t, kExtraN> kExtra{0xa1, 0xb2, 0xc3, 0xd4};

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetExtraBytes(as_bytes(span(kExtra)));
  ConstByteSpan extra = block->GetExtraBytes();
  EXPECT_EQ(extra.size(), kNumExtraBytes);
  EXPECT_EQ(std::memcmp(extra.data(), kExtra.data(), kNumExtraBytes), 0);
}

TEST(CustomBlockTest, SetAndGetTypedExtra) {
  constexpr size_t kN = 1024;
  using BlockType = Block<uintptr_t, sizeof(uint32_t)>;
  constexpr uint32_t kExtra = 0xa1b2c3d4;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetTypedExtra(kExtra);
  EXPECT_EQ(block->GetTypedExtra<uint32_t>(), kExtra);
}

TEST(CustomBlockTest, ExtraDataNotCopiedOnSplit) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  using BlockType = Block<uintptr_t, sizeof(uint32_t)>;
  constexpr uint32_t kExtra = 0xa1b2c3d4;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block1 = *result;
  block1->SetTypedExtra(kExtra);

  result = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block2 = *result;

  EXPECT_EQ(block1->GetTypedExtra<uint32_t>(), kExtra);
  EXPECT_EQ(block2->GetFlags(), 0U);
}

TEST(CustomBlockTest, ExtraDataPreservedByMergeNext) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  using BlockType = Block<uintptr_t, sizeof(uint32_t)>;
  constexpr uint32_t kExtra = 0xa1b2c3d4;

  std::array<std::byte, kN> bytes;
  Result<BlockType*> result = BlockType::Init(span(bytes));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  result = BlockType::Split(block, kSplitN);
  ASSERT_EQ(result.status(), OkStatus());

  block->SetTypedExtra(kExtra);
  EXPECT_EQ(BlockType::MergeNext(block), OkStatus());
  EXPECT_EQ(block->GetTypedExtra<uint32_t>(), kExtra);
}

}  // namespace pw::allocator
