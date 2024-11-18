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

#include "pw_allocator/first_fit_block_allocator.h"

#include <cstdint>

#include "lib/stdcompat/bit.h"
#include "pw_allocator/block_allocator_testing.h"
#include "pw_allocator/buffer.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::allocator::Layout;
using ::pw::allocator::test::Preallocation;
using FirstFitBlockAllocator =
    ::pw::allocator::FirstFitBlockAllocator<uint16_t>;
using BlockAllocatorTest =
    ::pw::allocator::test::BlockAllocatorTest<FirstFitBlockAllocator>;

class FirstFitBlockAllocatorTest : public BlockAllocatorTest {
 public:
  FirstFitBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  FirstFitBlockAllocator allocator_;
};

TEST_F(FirstFitBlockAllocatorTest, CanAutomaticallyInit) {
  FirstFitBlockAllocator allocator(GetBytes());
  CanAutomaticallyInit(allocator);
}

TEST_F(FirstFitBlockAllocatorTest, CanExplicitlyInit) {
  FirstFitBlockAllocator allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(FirstFitBlockAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(FirstFitBlockAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(FirstFitBlockAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(FirstFitBlockAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();

  alignas(BlockType::kAlignment) std::array<std::byte, kCapacity> buffer;
  pw::ByteSpan bytes(buffer);
  auto addr = cpp20::bit_cast<uintptr_t>(bytes.data());
  size_t offset = 64 - (addr % 64);
  offset += 4 * 12;
  offset %= 64;
  bytes = bytes.subspan(offset);
  FirstFitBlockAllocator allocator;
  allocator.Init(bytes);

  constexpr size_t kAlignment = 64;
  void* ptr0 = allocator.Allocate(Layout(kLargeInnerSize, kAlignment));
  ASSERT_NE(ptr0, nullptr);
  EXPECT_EQ(cpp20::bit_cast<uintptr_t>(ptr0) % kAlignment, 0U);
  UseMemory(ptr0, kLargeInnerSize);

  void* ptr1 = allocator.Allocate(Layout(kLargeInnerSize, kAlignment));
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(cpp20::bit_cast<uintptr_t>(ptr1) % kAlignment, 0U);
  UseMemory(ptr1, kLargeInnerSize);

  allocator.Deallocate(ptr0);
  allocator.Deallocate(ptr1);
}

TEST_F(FirstFitBlockAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(FirstFitBlockAllocatorTest, AllocatesFirstCompatible) {
  auto& allocator = GetAllocator({
      {kSmallOuterSize, Preallocation::kFree},
      {kSmallerOuterSize, Preallocation::kUsed},
      {kSmallOuterSize, Preallocation::kFree},
      {kSmallerOuterSize, Preallocation::kUsed},
      {kLargeOuterSize, Preallocation::kFree},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  Store(0, allocator.Allocate(Layout(kSmallInnerSize, 1)));
  EXPECT_EQ(NextAfter(0), Fetch(1));
  Store(4, allocator.Allocate(Layout(kLargeInnerSize, 1)));
  EXPECT_EQ(NextAfter(3), Fetch(4));
  EXPECT_EQ(NextAfter(4), Fetch(5));
}

TEST_F(FirstFitBlockAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(FirstFitBlockAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(FirstFitBlockAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(FirstFitBlockAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(FirstFitBlockAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(FirstFitBlockAllocatorTest, CanMeasureFragmentation) {
  CanMeasureFragmentation();
}

TEST_F(FirstFitBlockAllocatorTest, DisablePoisoning) {
  auto& allocator = GetAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();

  // Allocate 3 blocks to prevent the middle one from being merged when freed.
  std::array<void*, 3> ptrs;
  for (auto& ptr : ptrs) {
    ptr = allocator.Allocate(layout);
    ASSERT_NE(ptr, nullptr);
  }

  // Modify the contents of the block and check if it is still valid.
  auto* bytes = cpp20::bit_cast<std::byte*>(ptrs[1]);
  auto* block = BlockType::FromUsableSpace(bytes);
  allocator.Deallocate(bytes);
  EXPECT_TRUE(block->IsFree());
  EXPECT_TRUE(block->IsValid());
  bytes[0] = ~bytes[0];
  EXPECT_TRUE(block->IsValid());

  allocator.Deallocate(ptrs[0]);
  allocator.Deallocate(ptrs[2]);
}

TEST(PoisonedFirstFitBlockAllocatorTest, PoisonEveryFreeBlock) {
  using PoisonedFirstFitBlockAllocator =
      ::pw::allocator::FirstFitBlockAllocator<uintptr_t, 1>;
  using BlockType = PoisonedFirstFitBlockAllocator::BlockType;

  pw::allocator::WithBuffer<PoisonedFirstFitBlockAllocator,
                            FirstFitBlockAllocatorTest::kCapacity>
      allocator;
  allocator->Init(allocator.as_bytes());
  constexpr Layout layout =
      Layout::Of<std::byte[FirstFitBlockAllocatorTest::kSmallInnerSize]>();

  // Allocate 3 blocks to prevent the middle one from being merged when freed.
  std::array<void*, 3> ptrs;
  for (auto& ptr : ptrs) {
    ptr = allocator->Allocate(layout);
    ASSERT_NE(ptr, nullptr);
  }

  // Modify the contents of the block and check if it is still valid.
  auto* bytes = cpp20::bit_cast<std::byte*>(ptrs[1]);
  auto* block = BlockType::FromUsableSpace(bytes);
  allocator->Deallocate(bytes);

  EXPECT_TRUE(block->IsFree());
  EXPECT_TRUE(block->IsValid());
  bytes[0] = ~bytes[0];
  EXPECT_FALSE(block->IsValid());

  // Fix the block to prevent crashing on teardown.
  bytes[0] = ~bytes[0];
  allocator->Deallocate(ptrs[0]);
  allocator->Deallocate(ptrs[2]);
}

TEST(PoisonedFirstFitBlockAllocatorTest, PoisonPeriodically) {
  using PoisonedFirstFitBlockAllocator =
      ::pw::allocator::FirstFitBlockAllocator<uintptr_t, 4>;
  using BlockType = PoisonedFirstFitBlockAllocator::BlockType;

  pw::allocator::WithBuffer<PoisonedFirstFitBlockAllocator,
                            FirstFitBlockAllocatorTest::kCapacity>
      allocator;
  allocator->Init(allocator.as_bytes());
  constexpr Layout layout =
      Layout::Of<std::byte[FirstFitBlockAllocatorTest::kSmallInnerSize]>();

  // Allocate 9 blocks to prevent every other from being merged when freed.
  std::array<void*, 9> ptrs;
  for (auto& ptr : ptrs) {
    ptr = allocator->Allocate(layout);
    ASSERT_NE(ptr, nullptr);
  }

  for (size_t i = 1; i < ptrs.size(); i += 2) {
    auto* bytes = cpp20::bit_cast<std::byte*>(ptrs[i]);
    auto* block = BlockType::FromUsableSpace(bytes);
    allocator->Deallocate(bytes);
    EXPECT_TRUE(block->IsFree());
    EXPECT_TRUE(block->IsValid());
    bytes[0] = ~bytes[0];

    // Corruption is only detected on the fourth freed block.
    if (i == 7) {
      EXPECT_FALSE(block->IsValid());
      bytes[0] = ~bytes[0];
    } else {
      EXPECT_TRUE(block->IsValid());
    }
  }

  for (size_t i = 0; i < ptrs.size(); i += 2) {
    allocator->Deallocate(ptrs[i]);
  }
}

}  // namespace
