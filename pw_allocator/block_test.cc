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

#include "pw_allocator/block.h"
#include "pw_allocator/block_testing.h"
#include "pw_assert/check.h"
#include "pw_assert/internal/check_impl.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.
using ::pw::allocator::BlockResult;
using ::pw::allocator::Layout;
using ::pw::allocator::test::GetAlignedOffsetAfter;
using ::pw::allocator::test::GetOuterSize;
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
  size_t min_block = BlockType::kBlockOverhead + 1;
  size_t offset = GetAlignedOffsetAfter(
      bytes.data(), layout.alignment(), min_block + BlockType::kBlockOverhead);
  return min_block + offset;
}

/// Returns the largest offset into the given memory region at which a block
/// would have properly aligned usable space of the given size.
///
/// @pre ``bytes`` must not be smaller than the calculated offset plus
///      ``layout.size()``.
template <typename BlockType>
size_t GetLastAlignedOffset(pw::ByteSpan bytes, Layout layout) {
  size_t min_offset = GetFirstAlignedOffset<BlockType>(bytes, layout);
  return min_offset +
         pw::AlignDown(bytes.subspan(min_offset).size() - layout.size(),
                       layout.alignment());
}

/// Iterates to each block reachable from the given one and asserts that it is
/// valid.
template <typename BlockType>
void CheckAllReachableBlock(BlockType* block) {
  // Rewind to start.
  for (BlockType* prev = block->Prev(); prev != nullptr; prev = block->Prev()) {
    block = prev;
  }
  for (; block != nullptr; block = block->Next()) {
    block->CrashIfInvalid();
  }
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
  EXPECT_EQ(block->Next(), nullptr);
  CheckAllReachableBlock(block);
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

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirst_Null) {
  constexpr Layout kLayout(1, 1);

  BlockType* block = nullptr;

  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirst_ZeroSize) {
  constexpr Layout kLayout(0, 1);

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirst_Used) {
  constexpr Layout kLayout(1, 1);

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::FailedPrecondition());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirst_TooSmall) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the buffer so that the layout does not fit.
  pw::ByteSpan bytes = bytes_.subspan(
      0, GetOuterSize<BlockType>(kLayout.size()) - BlockType::kAlignment);

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_Exact_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that the first block is aligned.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead);
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for the requested block.
  size_t available = GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_Exact_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block so that the next block is aligned.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout);

  // Leave enough space free for the requested block.
  size_t available = GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_NewNext_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that the first block is aligned.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead);
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for the requested block and one more block.
  size_t available =
      GetOuterSize<BlockType>(kLayout.size()) + GetOuterSize<BlockType>(1);

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Allocate from the front of the block.
  BlockResult result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kSplitNew);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_NewNext_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block so that the next block is aligned.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout);

  // Leave enough space free for the requested block and one more block.
  size_t available =
      GetOuterSize<BlockType>(kLayout.size()) + GetOuterSize<BlockType>(1);

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kSplitNew);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_NewPrev_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that there is room for a block before the
  // first alignment boundary.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead) +
      kAlign - GetOuterSize<BlockType>(1);
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for a block and the requested block.
  size_t available =
      GetOuterSize<BlockType>(1) + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kSplitNew);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_NewPrev_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block with room for another block before the next
  // alignment boundary.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + kAlign -
                   GetOuterSize<BlockType>(1);

  // Leave enough space free for a block and the requested block.
  size_t available =
      GetOuterSize<BlockType>(1) + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kSplitNew);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_NewPrevAndNewNext_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that there is room for a block before the
  // first alignment boundary.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead) +
      kAlign - GetOuterSize<BlockType>(1);
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for a block, the requested block, and one more
  // block.
  size_t available = GetOuterSize<BlockType>(1) +
                     GetOuterSize<BlockType>(kLayout.size()) +
                     GetOuterSize<BlockType>(1);

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Allocate from the front of the block.
  BlockResult result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kSplitNew);
  EXPECT_EQ(result.next(), BlockResult::Next::kSplitNew);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_NewPrevAndNewNext_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block with room for another block before the next
  // alignment boundary.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + kAlign -
                   GetOuterSize<BlockType>(1);

  // Leave enough space free for a block and the requested block and one more
  // block.
  size_t available = kAlign + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  BlockResult result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kSplitNew);
  EXPECT_EQ(result.next(), BlockResult::Next::kSplitNew);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirst_ShiftToPrev_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that there is `kAlignment` bytes before
  // where the aligned block would start.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead) +
      kAlign - BlockType::kAlignment;
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for the `kAlignment` bytes and the requested block.
  size_t available =
      BlockType::kAlignment + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Attempt and fail to allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_ShiftToPrev_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block so that there is `kAlignment` bytes before
  // where the aligned block would start.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + kAlign -
                   BlockType::kAlignment;

  // Leave enough space free for the `kAlignment` bytes and the requested block.
  size_t available =
      BlockType::kAlignment + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kResized);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocFirst_ShiftToPrevAndNewNext_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that there is `kAlignment` bytes before
  // where the aligned block would start.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead) +
      kAlign - BlockType::kAlignment;
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for the `kAlignment` bytes, the requested block,
  // and one more block.
  size_t available = BlockType::kAlignment +
                     GetOuterSize<BlockType>(kLayout.size()) +
                     GetOuterSize<BlockType>(1);

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Attempt and fail to allocate from the front of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocFirst_ShiftToPrevAndNewNext_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block so that there is `kAlignment` bytes before
  // where the aligned block would start.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + kAlign -
                   BlockType::kAlignment;

  // Leave enough space free for the `kAlignment` bytes, the requested block,
  // and one more block.
  size_t available = BlockType::kAlignment +
                     GetOuterSize<BlockType>(kLayout.size()) +
                     GetOuterSize<BlockType>(1);

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Allocate from the front of the block.
  BlockResult result = BlockType::AllocFirst(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kResized);
  EXPECT_EQ(result.next(), BlockResult::Next::kSplitNew);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLast_Null) {
  constexpr Layout kLayout(1, 1);

  BlockType* block = nullptr;

  // Attempt and fail to allocate from the front of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLast_ZeroSize) {
  constexpr Layout kLayout(0, 1);

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  EXPECT_EQ(can_alloc_last.status(), pw::Status::InvalidArgument());

  // Attempt and fail to allocate from the front of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLast_Used) {
  constexpr Layout kLayout(1, 1);

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  EXPECT_EQ(can_alloc_last.status(), pw::Status::FailedPrecondition());

  // Attempt and fail to allocate from the front of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::FailedPrecondition());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLast_TooSmall) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the buffer so that the layout does not fit.
  pw::ByteSpan bytes = bytes_.subspan(
      0, GetOuterSize<BlockType>(kLayout.size()) - BlockType::kAlignment);

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  EXPECT_EQ(can_alloc_last.status(), pw::Status::OutOfRange());

  // Attempt and fail to allocate from the front of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLast_Exact_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that the first block is aligned.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead);
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for the requested block.
  size_t available = GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  ASSERT_EQ(can_alloc_last.status(), pw::OkStatus());
  EXPECT_EQ(can_alloc_last.size(), 0U);

  // Allocate from the back of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLast_Exact_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block so that the next block is aligned.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout);

  // Leave enough space free for the requested block.
  size_t available = GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  ASSERT_EQ(can_alloc_last.status(), pw::OkStatus());
  EXPECT_EQ(can_alloc_last.size(), 0U);

  // Allocate from the back of the block.
  BlockResult result = BlockType::AllocLast(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLast_NewPrev_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that there is room for a block before the
  // first alignment boundary.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead) +
      kAlign - GetOuterSize<BlockType>(1);
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for a block and the requested block.
  size_t available =
      GetOuterSize<BlockType>(1) + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  ASSERT_EQ(can_alloc_last.status(), pw::OkStatus());
  EXPECT_EQ(can_alloc_last.size(), GetOuterSize<BlockType>(1));

  // Allocate from the back of the block.
  BlockResult result = BlockType::AllocLast(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kSplitNew);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLast_NewPrev_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block with room for another block before the next
  // alignment boundary.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + kAlign -
                   GetOuterSize<BlockType>(1);

  // Leave enough space free for a block and the requested block.
  size_t available =
      GetOuterSize<BlockType>(1) + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  ASSERT_EQ(can_alloc_last.status(), pw::OkStatus());
  EXPECT_EQ(can_alloc_last.size(), GetOuterSize<BlockType>(1));

  // Allocate from the back of the block.
  auto result = BlockType::AllocLast(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kSplitNew);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CannotAllocLast_ShiftToPrev_FirstBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Trim the front of the buffer so that there is `kAlignment` bytes before
  // where the aligned block would start.
  size_t trim =
      GetAlignedOffsetAfter(bytes_.data(), kAlign, BlockType::kBlockOverhead) +
      kAlign - BlockType::kAlignment;
  pw::ByteSpan bytes = bytes_.subspan(trim);

  // Leave enough space free for the `kAlignment` bytes and the requested block.
  size_t available =
      BlockType::kAlignment + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes,
      {
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  EXPECT_EQ(can_alloc_last.status(), pw::Status::ResourceExhausted());

  // Attempt and fail to allocate from the back of the block.
  auto result = BlockType::AllocFirst(block, kLayout);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanAllocLast_ShiftToPrev_SubsequentBlock) {
  constexpr Layout kLayout(256, kAlign);

  // Preallocate a first block so that there is `kAlignment` bytes before
  // where the aligned block would start.
  size_t leading = GetFirstAlignedOffset<BlockType>(bytes_, kLayout) + kAlign -
                   BlockType::kAlignment;

  // Leave enough space free for the `kAlignment` bytes and the requested block.
  size_t available =
      BlockType::kAlignment + GetOuterSize<BlockType>(kLayout.size());

  auto* block = Preallocate<BlockType>(
      bytes_,
      {
          {leading, Preallocation::kUsed},
          {available, Preallocation::kFree},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  block = block->Next();

  // Check if we expect this to succeed.
  auto can_alloc_last = block->CanAllocLast(kLayout);
  ASSERT_EQ(can_alloc_last.status(), pw::OkStatus());
  EXPECT_EQ(can_alloc_last.size(), BlockType::kAlignment);

  // Allocate from the back of the block.
  BlockResult result = BlockType::AllocLast(block, kLayout);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kResized);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);

  EXPECT_GE(block->InnerSize(), kLayout.size());
  auto addr = reinterpret_cast<uintptr_t>(block->UsableSpace());
  EXPECT_EQ(addr % kAlign, 0U);
  EXPECT_TRUE(block->Used());
  CheckAllReachableBlock(block);
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
  CheckAllReachableBlock(block);
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
  CheckAllReachableBlock(block);
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
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithPrev) {
  constexpr size_t kOuterSize = 256;

  auto* first = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kFree},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kUsed},
      });
  BlockType* block = first->Next();
  BlockType* next = block->Next();
  BlockType::Free(block);
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_EQ(block->Next(), next);
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithNext) {
  constexpr size_t kOuterSize = 256;

  auto* first = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kUsed},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  BlockType* block = first->Next();
  BlockType* prev = block->Prev();
  BlockType::Free(block);
  EXPECT_FALSE(block->Used());
  EXPECT_EQ(block->Prev(), prev);
  EXPECT_TRUE(block->Last());
  CheckAllReachableBlock(block);
}

TEST_FOR_EACH_BLOCK_TYPE(CanFreeBlockAndMergeWithBoth) {
  constexpr size_t kOuterSize = 128;

  auto* first = Preallocate<BlockType>(
      bytes_,
      {
          {kOuterSize, Preallocation::kFree},
          {kOuterSize, Preallocation::kUsed},
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });
  BlockType* block = first->Next();
  BlockType::Free(block);
  EXPECT_EQ(block->Prev(), nullptr);
  EXPECT_TRUE(block->Last());
  CheckAllReachableBlock(block);
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

  BlockResult result = BlockType::Resize(block, block->InnerSize());
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);
  CheckAllReachableBlock(block);
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

  BlockResult result = BlockType::Resize(block, block->InnerSize());
  EXPECT_EQ(result.status(), pw::Status::FailedPrecondition());
  CheckAllReachableBlock(block);
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
  BlockResult result = BlockType::Resize(block, new_inner_size);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kResized);
  EXPECT_EQ(block->InnerSize(), new_inner_size);

  BlockType* next = block->Next();
  EXPECT_FALSE(next->Used());
  EXPECT_EQ(next->InnerSize(), next_inner_size + delta);
  CheckAllReachableBlock(block);
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
  BlockResult result = BlockType::Resize(block, new_inner_size);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kResized);
  EXPECT_EQ(block->InnerSize(), new_inner_size);

  BlockType* next = block->Next();
  EXPECT_FALSE(next->Used());
  EXPECT_EQ(next->InnerSize(), next_inner_size - delta);
  CheckAllReachableBlock(block);
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
  BlockResult result = BlockType::Resize(block, new_inner_size);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());
  CheckAllReachableBlock(block);
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
  BlockResult result = BlockType::Resize(block, new_inner_size);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kSplitNew);

  BlockType* next = block->Next();
  EXPECT_FALSE(next->Used());
  EXPECT_EQ(next->OuterSize(), delta);
  CheckAllReachableBlock(block);
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
  auto result = BlockType::Resize(block, new_inner_size);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());
}

TEST_FOR_EACH_BLOCK_TYPE(CannotResizeFromNull) {
  BlockType* block = nullptr;
  auto result = BlockType::Resize(block, 1);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
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
  ASSERT_TRUE(block->IsValid());

  block = block->Next();
  ASSERT_TRUE(block->IsValid());

  block = block->Next();
  ASSERT_TRUE(block->IsValid());
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

  auto result = BlockType::AllocLast(block1, kLayout1);
  ASSERT_EQ(result.status(), pw::OkStatus());

  BlockType* block2 = block1->Prev();
  result = BlockType::AllocLast(block2, kLayout2);
  ASSERT_EQ(result.status(), pw::OkStatus());

  EXPECT_EQ(block1->Alignment(), kAlign);
  EXPECT_EQ(block2->Alignment(), kAlign * 2);
}

TEST_FOR_EACH_BLOCK_TYPE(FreeBlocksHaveDefaultAlignment) {
  constexpr Layout kLayout1(128, kAlign);
  constexpr Layout kLayout2(384, kAlign * 2);

  auto* block1 = Preallocate<BlockType>(
      bytes_,
      {
          {Preallocation::kSizeRemaining, Preallocation::kFree},
      });

  auto result = BlockType::AllocLast(block1, kLayout1);
  ASSERT_EQ(result.status(), pw::OkStatus());

  BlockType* block2 = block1->Prev();
  result = BlockType::AllocLast(block2, kLayout2);
  ASSERT_EQ(result.status(), pw::OkStatus());

  EXPECT_EQ(block1->Alignment(), kAlign);
  BlockType::Free(block1);
  EXPECT_EQ(block1->Alignment(), BlockType::kAlignment);
}

}  // namespace
