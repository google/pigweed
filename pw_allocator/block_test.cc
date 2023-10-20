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

#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"
#include "pw_span/span.h"

using std::byte;

namespace pw::allocator {

const size_t kCapacity = 0x20000;

template <typename BlockType>
void CanCreateSingleBlock() {
  constexpr size_t kN = 200;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(block->OuterSize(), kN);
  EXPECT_EQ(block->InnerSize(), kN - BlockType::kBlockOverhead);
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_EQ(block->Next(), (BlockType*)((uintptr_t)block + kN));
  EXPECT_FALSE(block->Used());
  EXPECT_TRUE(block->Last());
}
TEST(GenericBlockTest, CanCreateSingleBlock) {
  CanCreateSingleBlock<Block<>>();
}
TEST(CustomBlockTest, CanCreateSingleBlock) {
  CanCreateSingleBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotCreateUnalignedSingleBlock() {
  constexpr size_t kN = 1024;

  // Force alignment, so we can un-force it below
  alignas(BlockType*) byte bytes[kN];
  byte* byte_ptr = bytes;

  Result<BlockType*> result = BlockType::Init(span(byte_ptr + 1, kN - 1));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::InvalidArgument());
}
TEST(GenericBlockTest, CannotCreateUnalignedSingleBlock) {
  CannotCreateUnalignedSingleBlock<Block<>>();
}
TEST(CustomBlockTest, CannotCreateUnalignedSingleBlock) {
  CannotCreateUnalignedSingleBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotCreateTooSmallBlock() {
  constexpr size_t kN = 2;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::ResourceExhausted());
}
TEST(GenericBlockTest, CannotCreateTooSmallBlock) {
  CannotCreateTooSmallBlock<Block<>>();
}
TEST(CustomBlockTest, CannotCreateTooSmallBlock) {
  CannotCreateTooSmallBlock<Block<uint32_t, kCapacity>>();
}

TEST(CustomBlockTest, CannotCreateTooLargeBlock) {
  using BlockType = Block<uint16_t, 512>;
  constexpr size_t kN = 1024;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

template <typename BlockType>
void CanSplitBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

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
  CanSplitBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CanSplitBlockUnaligned() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 513;

  alignas(BlockType*) byte bytes[kN];

  // We should split at sizeof(BlockType) + kSplitN bytes. Then
  // we need to round that up to an alignof(BlockType*) boundary.
  uintptr_t split_addr = ((uintptr_t)&bytes) + kSplitN;
  split_addr += alignof(BlockType*) - (split_addr % alignof(BlockType*));
  uintptr_t split_len = split_addr - (uintptr_t)&bytes;

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

  EXPECT_EQ(block1->InnerSize(), split_len);
  EXPECT_EQ(block1->OuterSize(), split_len + BlockType::kBlockOverhead);

  EXPECT_EQ(block2->OuterSize(), kN - block1->OuterSize());
  EXPECT_FALSE(block2->Used());

  EXPECT_EQ(block1->Next(), block2);
  EXPECT_EQ(block2->Prev(), block1);
}
TEST(GenericBlockTest, CanSplitBlockUnaligned) { CanSplitBlock<Block<>>(); }
TEST(CustomBlockTest, CanSplitBlockUnaligned) {
  CanSplitBlock<Block<uint32_t, kCapacity>>();
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
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

  Result<BlockType*> result3 = BlockType::Split(block1, kSplit2);
  ASSERT_EQ(result3.status(), OkStatus());
  BlockType* block3 = *result3;

  EXPECT_EQ(block1->Next(), block3);
  EXPECT_EQ(block3->Prev(), block1);
  EXPECT_EQ(block3->Next(), block2);
  EXPECT_EQ(block2->Prev(), block3);
}
TEST(GenericBlockTest, CanSplitMidBlock) { CanSplitMidBlock<Block<>>(); }
TEST(CustomBlockTest, CanSplitMidBlock) {
  CanSplitMidBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotSplitTooSmallBlock() {
  constexpr size_t kN = 64;
  constexpr size_t kSplitN = kN + 1;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  EXPECT_EQ(result2.status(), Status::OutOfRange());
}

template <typename BlockType>
void CannotSplitBlockWithoutHeaderSpace() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = kN - BlockType::kBlockOverhead - 1;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  EXPECT_EQ(result2.status(), Status::ResourceExhausted());
}
TEST(GenericBlockTest, CannotSplitBlockWithoutHeaderSpace) {
  CannotSplitBlockWithoutHeaderSpace<Block<>>();
}
TEST(CustomBlockTest, CannotSplitBlockWithoutHeaderSpace) {
  CannotSplitBlockWithoutHeaderSpace<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotMakeBlockLargerInSplit() {
  // Ensure that we can't ask for more space than the block actually has...
  constexpr size_t kN = 1024;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 =
      BlockType::Split(block1, block1->InnerSize() + 1);
  EXPECT_EQ(result2.status(), Status::OutOfRange());
}
TEST(GenericBlockTest, CannotMakeBlockLargerInSplit) {
  CannotMakeBlockLargerInSplit<Block<>>();
}
TEST(CustomBlockTest, CannotMakeBlockLargerInSplit) {
  CannotMakeBlockLargerInSplit<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotMakeSecondBlockLargerInSplit() {
  // Ensure that the second block in split is at least of the size of header.
  constexpr size_t kN = 1024;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(
      block1, block1->InnerSize() - BlockType::kBlockOverhead + 1);
  EXPECT_EQ(result2.status(), Status::ResourceExhausted());
}
TEST(GenericBlockTest, CannotMakeSecondBlockLargerInSplit) {
  CannotMakeSecondBlockLargerInSplit<Block<>>();
}
TEST(CustomBlockTest, CannotMakeSecondBlockLargerInSplit) {
  CannotMakeSecondBlockLargerInSplit<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CanMakeZeroSizeFirstBlock() {
  // This block does support splitting with zero payload size.
  constexpr size_t kN = 1024;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, 0);
  ASSERT_EQ(result2.status(), OkStatus());
  EXPECT_EQ(block1->InnerSize(), static_cast<size_t>(0));
}
TEST(GenericBlockTest, CanMakeZeroSizeFirstBlock) {
  CanMakeZeroSizeFirstBlock<Block<>>();
}
TEST(CustomBlockTest, CanMakeZeroSizeFirstBlock) {
  CanMakeZeroSizeFirstBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CanMakeZeroSizeSecondBlock() {
  // Likewise, the split block can be zero-width.
  constexpr size_t kN = 1024;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 =
      BlockType::Split(block1, block1->InnerSize() - BlockType::kBlockOverhead);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

  EXPECT_EQ(block2->InnerSize(), static_cast<size_t>(0));
}
TEST(GenericBlockTest, CanMakeZeroSizeSecondBlock) {
  CanMakeZeroSizeSecondBlock<Block<>>();
}
TEST(CustomBlockTest, CanMakeZeroSizeSecondBlock) {
  CanMakeZeroSizeSecondBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CanMarkBlockUsed() {
  constexpr size_t kN = 1024;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->MarkUsed();
  EXPECT_TRUE(block->Used());

  // Mark used packs that data into the next pointer. Check that it's still
  // valid
  EXPECT_EQ(block->Next(), (BlockType*)((uintptr_t)block + kN));

  block->MarkFree();
  EXPECT_FALSE(block->Used());
}
TEST(GenericBlockTest, CanMarkBlockUsed) { CanMarkBlockUsed<Block<>>(); }
TEST(CustomBlockTest, CanMarkBlockUsed) {
  CanMarkBlockUsed<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotSplitUsedBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  block1->MarkUsed();
  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  EXPECT_EQ(result2.status(), Status::FailedPrecondition());
}
TEST(GenericBlockTest, CannotSplitUsedBlock) {
  CannotSplitUsedBlock<Block<>>();
}
TEST(CustomBlockTest, CannotSplitUsedBlock) {
  CannotSplitUsedBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CanMergeWithNextBlock() {
  // Do the three way merge from "CanSplitMidBlock", and let's
  // merge block 3 and 2
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result2.status(), OkStatus());

  Result<BlockType*> result3 = BlockType::Split(block1, kSplit2);
  ASSERT_EQ(result3.status(), OkStatus());
  BlockType* block3 = *result3;

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
  CanMergeWithNextBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotMergeWithFirstOrLastBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  alignas(BlockType*) byte bytes[kN];

  // Do a split, just to check that the checks on Next/Prev are
  // different...
  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

  EXPECT_EQ(BlockType::MergeNext(block2), Status::OutOfRange());

  BlockType* block0 = block1->Prev();
  EXPECT_EQ(BlockType::MergeNext(block0), Status::OutOfRange());
}
TEST(GenericBlockTest, CannotMergeWithFirstOrLastBlock) {
  CannotMergeWithFirstOrLastBlock<Block<>>();
}
TEST(CustomBlockTest, CannotMergeWithFirstOrLastBlock) {
  CannotMergeWithFirstOrLastBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CannotMergeUsedBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  alignas(BlockType*) byte bytes[kN];

  // Do a split, just to check that the checks on Next/Prev are
  // different...
  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result2.status(), OkStatus());

  block1->MarkUsed();
  EXPECT_EQ(BlockType::MergeNext(block1), Status::FailedPrecondition());
}
TEST(GenericBlockTest, CannotMergeUsedBlock) {
  CannotMergeUsedBlock<Block<>>();
}
TEST(CustomBlockTest, CannotMergeUsedBlock) {
  CannotMergeUsedBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CanCheckValidBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 256;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

  Result<BlockType*> result3 = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result3.status(), OkStatus());
  BlockType* block3 = *result3;

  EXPECT_TRUE(block1->IsValid());
  block1->CrashIfInvalid();

  EXPECT_TRUE(block2->IsValid());
  block2->CrashIfInvalid();

  EXPECT_TRUE(block3->IsValid());
  block3->CrashIfInvalid();
}
TEST(GenericBlockTest, CanCheckValidBlock) { CanCheckValidBlock<Block<>>(); }
TEST(CustomBlockTest, CanCheckValidBlock) {
  CanCheckValidBlock<Block<uint32_t, kCapacity>>();
}

template <typename BlockType>
void CanCheckInvalidBlock() {
  constexpr size_t kN = 1024;
  constexpr size_t kSplit1 = 512;
  constexpr size_t kSplit2 = 128;
  constexpr size_t kSplit3 = 256;
  alignas(BlockType*) byte bytes[kN];
  memset(bytes, 0, sizeof(bytes));

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplit1);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

  Result<BlockType*> result3 = BlockType::Split(block2, kSplit2);
  ASSERT_EQ(result3.status(), OkStatus());
  BlockType* block3 = *result3;

  Result<BlockType*> result4 = BlockType::Split(block3, kSplit3);
  ASSERT_EQ(result4.status(), OkStatus());

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
  CanCheckInvalidBlock<Block<uint32_t, kCapacity>>();
}

TEST(CustomBlockTest, CustomFlagsInitiallyZero) {
  constexpr size_t kN = 1024;
  using BlockType = Block<uint16_t, kN>;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  EXPECT_EQ(block->GetFlags(), 0U);
}

TEST(CustomBlockTest, SetCustomFlags) {
  constexpr size_t kN = 1024;
  using BlockType = Block<uint16_t, kN>;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetFlags(1);
  EXPECT_EQ(block->GetFlags(), 1U);
}

TEST(CustomBlockTest, SetAllCustomFlags) {
  constexpr size_t kN = 1024;
  using BlockType = Block<uint16_t, kN>;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
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
  using BlockType = Block<uint16_t, kN>;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result.status(), OkStatus());
  BlockType* block = *result;

  block->SetFlags(0x155);
  block->SetFlags(0x2AA, 0x333);
  EXPECT_EQ(block->GetFlags(), 0x2EEU);
}

TEST(CustomBlockTest, FlagsNotCopiedOnSplit) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  using BlockType = Block<uint16_t, kN>;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;
  block1->SetFlags(0x137);

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result2.status(), OkStatus());
  BlockType* block2 = *result2;

  EXPECT_EQ(block1->GetFlags(), 0x137U);
  EXPECT_EQ(block2->GetFlags(), 0U);
}

TEST(CustomBlockTest, FlagsPreservedByMergeNext) {
  constexpr size_t kN = 1024;
  constexpr size_t kSplitN = 512;
  using BlockType = Block<uint16_t, kN>;
  alignas(BlockType*) byte bytes[kN];

  Result<BlockType*> result1 = BlockType::Init(span(bytes, kN));
  ASSERT_EQ(result1.status(), OkStatus());
  BlockType* block1 = *result1;

  Result<BlockType*> result2 = BlockType::Split(block1, kSplitN);
  ASSERT_EQ(result2.status(), OkStatus());

  block1->SetFlags(0x137);
  EXPECT_EQ(BlockType::MergeNext(block1), OkStatus());
  EXPECT_EQ(block1->GetFlags(), 0x137U);
}

}  // namespace pw::allocator
