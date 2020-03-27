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

#include "gtest/gtest.h"
#include "pw_span/span.h"

using std::byte;

namespace pw::allocator {

TEST(Block, CanCreateSingleBlock) {
  constexpr size_t kN = 200;
  alignas(Block*) byte bytes[kN];

  Block* block = nullptr;
  auto status = Block::Init(span(bytes, kN), &block);

  ASSERT_EQ(status, Status::OK);
  EXPECT_EQ(block->OuterSize(), kN);
  EXPECT_EQ(block->InnerSize(), kN - sizeof(Block));
  EXPECT_EQ(block->PrevBlock(), nullptr);
  EXPECT_EQ(block->NextBlock(), (Block*)((uintptr_t)block + kN));
  EXPECT_EQ(block->Used(), false);
  EXPECT_EQ(block->Last(), true);
}

TEST(Block, CannotCreateUnalignedSingleBlock) {
  constexpr size_t kN = 1024;

  // Force alignment, so we can un-force it below
  alignas(Block*) byte bytes[kN];
  byte* byte_ptr = bytes;

  Block* block = nullptr;
  auto status = Block::Init(span(byte_ptr + 1, kN - 1), &block);

  EXPECT_EQ(status, Status::INVALID_ARGUMENT);
}

TEST(Block, CannotCreateTooSmallBlock) {
  constexpr size_t kN = 2;
  alignas(Block*) byte bytes[kN];
  Block* block = nullptr;
  auto status = Block::Init(span(bytes, kN), &block);

  EXPECT_EQ(status, Status::INVALID_ARGUMENT);
}

TEST(Block, CanSplitBlock) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  alignas(Block*) byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  auto status = block->Split(kSplitN, &next_block);

  ASSERT_EQ(status, Status::OK);
  EXPECT_EQ(block->InnerSize(), kSplitN);
  EXPECT_EQ(block->OuterSize(), kSplitN + sizeof(Block));
  EXPECT_EQ(block->Last(), false);

  EXPECT_EQ(next_block->OuterSize(), kN - kSplitN - sizeof(Block));
  EXPECT_EQ(next_block->Used(), false);
  EXPECT_EQ(next_block->Last(), true);

  EXPECT_EQ(block->NextBlock(), next_block);
  EXPECT_EQ(next_block->PrevBlock(), block);
}

TEST(Block, CanSplitBlockUnaligned) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 513;

  alignas(Block*) byte bytes[kN];

  // We should split at sizeof(Block) + kSplitN bytes. Then
  // we need to round that up to an alignof(Block*) boundary.
  uintptr_t split_addr = ((uintptr_t)&bytes) + kSplitN;
  split_addr += alignof(Block*) - (split_addr % alignof(Block*));
  uintptr_t split_len = split_addr - (uintptr_t)&bytes;

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  auto status = block->Split(kSplitN, &next_block);

  ASSERT_EQ(status, Status::OK);
  EXPECT_EQ(block->InnerSize(), split_len);
  EXPECT_EQ(block->OuterSize(), split_len + sizeof(Block));
  EXPECT_EQ(next_block->OuterSize(), kN - split_len - sizeof(Block));
  EXPECT_EQ(next_block->Used(), false);
  EXPECT_EQ(block->NextBlock(), next_block);
  EXPECT_EQ(next_block->PrevBlock(), block);
}

TEST(Block, CanSplitMidBlock) {
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
  byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* block2 = nullptr;
  block->Split(kSplit1, &block2);

  Block* block3 = nullptr;
  block->Split(kSplit2, &block3);

  EXPECT_EQ(block->NextBlock(), block3);
  EXPECT_EQ(block3->NextBlock(), block2);
  EXPECT_EQ(block2->PrevBlock(), block3);
  EXPECT_EQ(block3->PrevBlock(), block);
}

TEST(Block, CannotSplitBlockWithoutHeaderSpace) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = kN - sizeof(Block) - 1;
  byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  auto status = block->Split(kSplitN, &next_block);

  EXPECT_EQ(status, Status::RESOURCE_EXHAUSTED);
  EXPECT_EQ(next_block, nullptr);
}

TEST(Block, MustProvideNextBlockPointer) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = kN - sizeof(Block) - 1;
  byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  auto status = block->Split(kSplitN, nullptr);
  EXPECT_EQ(status, Status::INVALID_ARGUMENT);
}

TEST(Block, CannotMakeBlockLargerInSplit) {
  // Ensure that we can't ask for more space than the block actually has...
  constexpr size_t kN = 1024;
  byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  auto status = block->Split(block->InnerSize() + 1, &next_block);

  EXPECT_EQ(status, Status::OUT_OF_RANGE);
}

TEST(Block, CanMakeZeroSizeFirstBlock) {
  // This block does support splitting with zero payload size.
  constexpr size_t kN = 1024;
  byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  auto status = block->Split(0, &next_block);

  ASSERT_EQ(status, Status::OK);
  EXPECT_EQ(block->InnerSize(), static_cast<size_t>(0));
}

TEST(Block, CanMakeZeroSizeSecondBlock) {
  // Likewise, the split block can be zero-width.
  constexpr size_t kN = 1024;
  byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  auto status = block->Split(block->InnerSize() - sizeof(Block), &next_block);

  ASSERT_EQ(status, Status::OK);
  EXPECT_EQ(next_block->InnerSize(), static_cast<size_t>(0));
}

TEST(Block, CanMarkBlockUsed) {
  constexpr size_t kN = 1024;
  alignas(Block*) byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  block->MarkUsed();
  EXPECT_EQ(block->Used(), true);

  // Mark used packs that data into the next pointer. Check that it's still
  // valid
  EXPECT_EQ(block->NextBlock(), (Block*)((uintptr_t)block + kN));

  block->MarkFree();
  EXPECT_EQ(block->Used(), false);
}

TEST(Block, CannotSplitUsedBlock) {
  constexpr size_t kN = 1024;
  alignas(Block*) byte bytes[kN];

  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  block->MarkUsed();

  Block* next_block = nullptr;
  auto status = block->Split(512, &next_block);
  EXPECT_EQ(status, Status::FAILED_PRECONDITION);
}

TEST(Block, CanMergeWithNextBlock) {
  // Do the three way merge from "CanSplitMidBlock", and let's
  // merge block 3 and 2
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;
  byte bytes[kN];

  Block* block = nullptr;
  Block::Init(pw::span(bytes, kN), &block);

  Block* block2 = nullptr;
  block->Split(kSplit1, &block2);

  Block* block3 = nullptr;
  block->Split(kSplit2, &block3);

  EXPECT_EQ(block3->MergeNext(), Status::OK);

  EXPECT_EQ(block->NextBlock(), block3);
  EXPECT_EQ(block3->PrevBlock(), block);
  EXPECT_EQ(block->InnerSize(), kSplit2);

  // The resulting "right hand" block should have an outer size of 1024 - 256 -
  // sizeof(Block), which accounts for the first block.
  EXPECT_EQ(block3->OuterSize(), kN - kSplit2 - sizeof(Block));
}

TEST(Block, CannotMergeWithFirstOrLastBlock) {
  constexpr size_t kN = 1024;
  byte bytes[kN];

  // Do a split, just to sanity check that the checks on Next/Prev are
  // different...
  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  block->Split(512, &next_block);

  EXPECT_EQ(next_block->MergeNext(), Status::OUT_OF_RANGE);
  EXPECT_EQ(block->MergePrev(), Status::OUT_OF_RANGE);
}

TEST(Block, CannotMergeUsedBlock) {
  constexpr size_t kN = 1024;
  byte bytes[kN];

  // Do a split, just to sanity check that the checks on Next/Prev are
  // different...
  Block* block = nullptr;
  Block::Init(span(bytes, kN), &block);

  Block* next_block = nullptr;
  block->Split(512, &next_block);

  block->MarkUsed();
  EXPECT_EQ(block->MergeNext(), Status::FAILED_PRECONDITION);
  EXPECT_EQ(next_block->MergePrev(), Status::FAILED_PRECONDITION);
}

}  // namespace pw::allocator
