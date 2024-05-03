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

#include "pw_allocator/worst_fit_block_allocator.h"

#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

using ::pw::allocator::Layout;
using ::pw::allocator::test::BlockAllocatorTest;
using ::pw::allocator::test::Preallocation;
using WorstFitBlockAllocatorType =
    ::pw::allocator::WorstFitBlockAllocator<BlockAllocatorTest::OffsetType>;

class WorstFitBlockAllocatorTest : public BlockAllocatorTest {
 public:
  WorstFitBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  WorstFitBlockAllocatorType allocator_;
};

// Unit tests.

TEST_F(WorstFitBlockAllocatorTest, CanAutomaticallyInit) {
  WorstFitBlockAllocatorType allocator(GetBytes());
  CanAutomaticallyInit(allocator);
}

TEST_F(WorstFitBlockAllocatorTest, CanExplicitlyInit) {
  WorstFitBlockAllocatorType allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(WorstFitBlockAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(WorstFitBlockAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(WorstFitBlockAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(WorstFitBlockAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(WorstFitBlockAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(WorstFitBlockAllocatorTest, AllocatesWorstCompatible) {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 3},
      {kSmallerOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 5},
      {kLargerOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 7},
  });

  Store(6, allocator.Allocate(Layout(kLargeInnerSize, 1)));
  EXPECT_EQ(NextAfter(5), Fetch(6));
  EXPECT_EQ(NextAfter(6), Fetch(7));
  Store(0, allocator.Allocate(Layout(kSmallInnerSize, 1)));
  EXPECT_EQ(NextAfter(0), Fetch(1));
}

TEST_F(WorstFitBlockAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(WorstFitBlockAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(WorstFitBlockAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(WorstFitBlockAllocatorTest, CanGetLayoutFromValidPointer) {
  CanGetLayoutFromValidPointer();
}

TEST_F(WorstFitBlockAllocatorTest, CannotGetLayoutFromInvalidPointer) {
  CannotGetLayoutFromInvalidPointer();
}

}  // namespace
