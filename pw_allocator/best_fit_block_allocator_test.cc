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

#include "pw_allocator/best_fit_block_allocator.h"

#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

// Test fixtures.

using BlockAllocatorTest = test::BlockAllocatorTest;
using Preallocation = test::Preallocation;
using BestFitBlockAllocatorType =
    BestFitBlockAllocator<BlockAllocatorTest::OffsetType>;

class BestFitBlockAllocatorTest : public BlockAllocatorTest {
 public:
  BestFitBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  BestFitBlockAllocatorType allocator_;
};

// Unit tests.

TEST_F(BestFitBlockAllocatorTest, CanAutomaticallyInit) {
  BestFitBlockAllocatorType allocator(GetBytes());
  CanAutomaticallyInit(allocator);
}

TEST_F(BestFitBlockAllocatorTest, CanExplicitlyInit) {
  BestFitBlockAllocatorType allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(BestFitBlockAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(BestFitBlockAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(BestFitBlockAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(BestFitBlockAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(BestFitBlockAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(BestFitBlockAllocatorTest, AllocatesBestCompatible) {
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

  Store(2, allocator.Allocate(Layout(kSmallInnerSize, 1)));
  EXPECT_EQ(NextAfter(1), Fetch(2));
  EXPECT_EQ(NextAfter(2), Fetch(3));
  Store(0, allocator.Allocate(Layout(kLargeInnerSize, 1)));
  EXPECT_EQ(NextAfter(0), Fetch(1));
}

TEST_F(BestFitBlockAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(BestFitBlockAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(BestFitBlockAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(BestFitBlockAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(BestFitBlockAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(BestFitBlockAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(BestFitBlockAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(BestFitBlockAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(BestFitBlockAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(BestFitBlockAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(BestFitBlockAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(BestFitBlockAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(BestFitBlockAllocatorTest, CanGetLayoutFromValidPointer) {
  CanGetLayoutFromValidPointer();
}

TEST_F(BestFitBlockAllocatorTest, CannotGetLayoutFromInvalidPointer) {
  CannotGetLayoutFromInvalidPointer();
}

}  // namespace
}  // namespace pw::allocator
