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

#include "pw_allocator/dual_first_fit_block_allocator.h"

#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

using ::pw::allocator::Layout;
using ::pw::allocator::test::BlockAllocatorTest;
using ::pw::allocator::test::Preallocation;
using DualFirstFitBlockAllocatorType =
    ::pw::allocator::DualFirstFitBlockAllocator<BlockAllocatorTest::OffsetType>;

// Minimum size of a "large" allocation; allocation less than this size are
// considered "small" when using the DualFirstFit strategy.
static constexpr size_t kDualFitThreshold =
    BlockAllocatorTest::kSmallInnerSize * 2;

class DualFirstFitBlockAllocatorTest : public BlockAllocatorTest {
 public:
  DualFirstFitBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  DualFirstFitBlockAllocatorType allocator_;
};

// Unit tests.

TEST_F(DualFirstFitBlockAllocatorTest, CanAutomaticallyInit) {
  DualFirstFitBlockAllocatorType allocator(GetBytes(), kDualFitThreshold);
  CanAutomaticallyInit(allocator);
}

TEST_F(DualFirstFitBlockAllocatorTest, CanExplicitlyInit) {
  DualFirstFitBlockAllocatorType allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(DualFirstFitBlockAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(DualFirstFitBlockAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(DualFirstFitBlockAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(DualFirstFitBlockAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(DualFirstFitBlockAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(DualFirstFitBlockAllocatorTest, AllocatesUsingThreshold) {
  auto& allocator = GetAllocator({
      {kLargerOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 3},
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 5},
      {kSmallOuterSize, Preallocation::kIndexFree},
  });
  auto& dual_first_fit_block_allocator =
      static_cast<DualFirstFitBlockAllocatorType&>(allocator);
  dual_first_fit_block_allocator.set_threshold(kDualFitThreshold);

  Store(0, allocator.Allocate(Layout(kLargeInnerSize, 1)));
  EXPECT_EQ(NextAfter(0), Fetch(1));
  Store(4, allocator.Allocate(Layout(kLargeInnerSize, 1)));
  EXPECT_EQ(NextAfter(3), Fetch(4));
  EXPECT_EQ(NextAfter(4), Fetch(5));
  Store(6, allocator.Allocate(Layout(kSmallInnerSize, 1)));
  EXPECT_EQ(NextAfter(5), Fetch(6));
  EXPECT_EQ(NextAfter(6), Fetch(7));
  Store(2, allocator.Allocate(Layout(kSmallInnerSize, 1)));
  EXPECT_EQ(NextAfter(1), Fetch(2));
  EXPECT_EQ(NextAfter(2), Fetch(3));
}

TEST_F(DualFirstFitBlockAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(DualFirstFitBlockAllocatorTest, DeallocateShuffled) {
  DeallocateShuffled();
}

TEST_F(DualFirstFitBlockAllocatorTest, IterateOverBlocks) {
  IterateOverBlocks();
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(DualFirstFitBlockAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(DualFirstFitBlockAllocatorTest, ResizeLargeSmaller) {
  ResizeLargeSmaller();
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeLargeLarger) {
  ResizeLargeLarger();
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(DualFirstFitBlockAllocatorTest, ResizeSmallSmaller) {
  ResizeSmallSmaller();
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeSmallLarger) {
  ResizeSmallLarger();
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(DualFirstFitBlockAllocatorTest, CanGetLayoutFromValidPointer) {
  CanGetLayoutFromValidPointer();
}

TEST_F(DualFirstFitBlockAllocatorTest, CannotGetLayoutFromInvalidPointer) {
  CannotGetLayoutFromInvalidPointer();
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeLargeSmallerAcrossThreshold) {
  auto& allocator = GetAllocator({{kDualFitThreshold * 2, 0}});
  // Shrinking succeeds, and the pointer is unchanged even though it is now
  // below the threshold.
  size_t new_size = kDualFitThreshold / 2;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kDualFitThreshold / 2);
}

TEST_F(DualFirstFitBlockAllocatorTest, ResizeSmallLargerAcrossThreshold) {
  auto& allocator = GetAllocator({
      {Preallocation::kSizeRemaining, Preallocation::kIndexNext},
      {kDualFitThreshold / 2, 1},
      {kDualFitThreshold * 2, Preallocation::kIndexFree},
  });
  // Growing succeeds, and the pointer is unchanged even though it is now
  // above the threshold.
  size_t new_size = kDualFitThreshold * 2;
  ASSERT_TRUE(allocator.Resize(Fetch(1), new_size));
  UseMemory(Fetch(1), kDualFitThreshold * 2);
}

}  // namespace
