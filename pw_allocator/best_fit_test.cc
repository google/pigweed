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

#include "pw_allocator/best_fit.h"

#include "pw_allocator/best_fit_block_allocator.h"
#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

using ::pw::allocator::Layout;
using ::pw::allocator::test::BlockAllocatorTest;
using ::pw::allocator::test::Preallocation;
using BlockType = ::pw::allocator::BestFitBlock<uint16_t>;
using BestFitAllocator = ::pw::allocator::BestFitAllocator<BlockType>;

class BestFitAllocatorTest : public BlockAllocatorTest<BestFitAllocator> {
 public:
  BestFitAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  BestFitAllocator allocator_;
};

// Unit tests.

TEST_F(BestFitAllocatorTest, AutomaticallyInit) {
  BestFitAllocator allocator(GetBytes());
  AutomaticallyInit(allocator);
}

TEST_F(BestFitAllocatorTest, ExplicitlyInit) {
  BestFitAllocator allocator;
  ExplicitlyInit(allocator);
}

TEST_F(BestFitAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(BestFitAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(BestFitAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(BestFitAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(BestFitAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(BestFitAllocatorTest, AllocatesBestCompatible) {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, Preallocation::kFree},
      {kSmallerOuterSize, Preallocation::kUsed},
      {kSmallOuterSize, Preallocation::kFree},
      {kSmallerOuterSize, Preallocation::kUsed},
      {kLargerOuterSize, Preallocation::kFree},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  void* ptr1 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(Fetch(1), ptr1);
  EXPECT_LT(ptr1, Fetch(3));

  void* ptr2 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(ptr2, Fetch(1));

  // A second small block fits in the leftovers of the first "Large" block.
  void* ptr3 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(ptr3, Fetch(1));

  allocator.Deallocate(ptr1);
  allocator.Deallocate(ptr2);
  allocator.Deallocate(ptr3);
}

TEST_F(BestFitAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(BestFitAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(BestFitAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(BestFitAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(BestFitAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(BestFitAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(BestFitAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(BestFitAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(BestFitAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(BestFitAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(BestFitAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(BestFitAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(BestFitAllocatorTest, MeasureFragmentation) { MeasureFragmentation(); }

TEST_F(BestFitAllocatorTest, PoisonPeriodically) { PoisonPeriodically(); }

// TODO(b/376730645): Remove this test when the legacy alias is deprecated.
using BestFitBlockAllocator = ::pw::allocator::BestFitBlockAllocator<uint16_t>;
class BestFitBlockAllocatorTest
    : public BlockAllocatorTest<BestFitBlockAllocator> {
 public:
  BestFitBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  BestFitBlockAllocator allocator_;
};
TEST_F(BestFitBlockAllocatorTest, AllocatesBestCompatible) {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, Preallocation::kFree},
      {kSmallerOuterSize, Preallocation::kUsed},
      {kSmallOuterSize, Preallocation::kFree},
      {kSmallerOuterSize, Preallocation::kUsed},
      {kLargerOuterSize, Preallocation::kFree},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  void* ptr1 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(Fetch(1), ptr1);
  EXPECT_LT(ptr1, Fetch(3));

  void* ptr2 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(ptr2, Fetch(1));

  // A second small block fits in the leftovers of the first "Large" block.
  void* ptr3 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(ptr3, Fetch(1));

  allocator.Deallocate(ptr1);
  allocator.Deallocate(ptr2);
  allocator.Deallocate(ptr3);
}

}  // namespace
