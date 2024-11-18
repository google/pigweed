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
#pragma once

#include <cstddef>
#include <limits>

#include "pw_allocator/buffer.h"
#include "pw_allocator/bump_allocator.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator::test {

/// Test fixture for testing Buckets.
///
/// This class contains code both to set up a bucket and a number of free
/// blocks that can be stored in it, as well as unit test methods that apply to
/// all buckets.
template <typename BucketType>
class BucketTest : public ::testing::Test {
 protected:
  using BlockType = typename BucketType::BlockType;
  using ItemType = typename BucketType::ItemType;

  constexpr static size_t kMaxBlocks = 4;
  static constexpr Layout kLayout1 = Layout(0x040, 1);
  static constexpr Layout kLayout2 = Layout(0x080, 2);
  static constexpr Layout kLayout3 = Layout(0x100, 4);
  static constexpr Layout kLayout4 = Layout(0x200, 8);
  constexpr static size_t kCapacity = kLayout4.size() * 4;

  BucketType& bucket() { return bucket_; }

  // Test fixtures.

  void SetUp() override {
    auto result = BlockType::Init(bytes_);
    PW_ASSERT(result.status() == pw::OkStatus());
    available_ = *result;
  }

  /// Returns a layout with the same alignment as the given `layout`, and a size
  /// that is one less.
  static constexpr Layout ShrinkByOne(Layout layout) {
    return Layout(layout.size() - 1, layout.alignment());
  }

  /// Creates a free block of the given inner size.
  ///
  /// This creates a free block that can be added to a bucket, and a guard
  /// block that remains allocated to prevent the free blocks from merging.
  /// It avoids using any block allocator, and the buckets those type may use.
  /// Instead, it manages the blocks directly from a block representing the
  /// remaining available memory.
  BlockType& CreateBlock(Layout layout) {
    auto result = BlockType::AllocFirst(std::move(available_), layout);
    PW_ASSERT(result.status() == pw::OkStatus());
    BlockType* block = result.block();
    available_ = block->Next();

    result = BlockType::AllocFirst(std::move(available_), Layout(1, 1));
    PW_ASSERT(result.status() == pw::OkStatus());
    BlockType* guard = result.block();
    available_ = guard->Next();

    result = BlockType::Free(std::move(block));
    PW_ASSERT(result.ok());
    return *(result.block());
  }

  /// Creates a free block of the given inner size, and adds it to the test
  /// bucket.
  BlockType& CreateBlockAndAddToBucket(Layout layout) {
    BlockType& block = CreateBlock(layout);
    PW_ASSERT(bucket_.Add(block));
    return block;
  }

  // Unit tests.
  void SetsAndGetsMaxInnerSize() {
    EXPECT_EQ(bucket_.max_inner_size(), std::numeric_limits<size_t>::max());
    bucket_.set_max_inner_size(kLayout1.size());
    EXPECT_EQ(bucket_.max_inner_size(), kLayout1.size());
  }

  void AddsAndRemovesBlocks() {
    BlockType& block1 = CreateBlockAndAddToBucket(kLayout1);
    BlockType& block2 = CreateBlockAndAddToBucket(kLayout2);
    BlockType& block3 = CreateBlockAndAddToBucket(kLayout3);
    BlockType& block4 = CreateBlockAndAddToBucket(kLayout4);
    EXPECT_TRUE(bucket_.Remove(block1));
    EXPECT_TRUE(bucket_.Remove(block2));
    EXPECT_TRUE(bucket_.Remove(block3));
    EXPECT_TRUE(bucket_.Remove(block4));
    EXPECT_TRUE(bucket_.empty());
  }

  void FailsToAddWhenBlockIsTooSmall() {
    // Create the smallest block possible.
    BlockType& block = CreateBlock(Layout(1, 1));

    // Some allocators may not be able to create blocks with inner sizes smaller
    // than the bucket's intrusive item type.
    if (block.InnerSize() < sizeof(ItemType)) {
      EXPECT_FALSE(bucket_.Add(block));
    }
  }

  void FailsToRemoveBlockWhenNotFound() {
    BlockType& block1 = CreateBlockAndAddToBucket(kLayout1);
    BlockType& block2 = CreateBlockAndAddToBucket(kLayout2);
    BlockType& block3 = CreateBlockAndAddToBucket(kLayout3);
    BlockType& block4 = CreateBlockAndAddToBucket(kLayout4);
    bucket_.Clear();
    EXPECT_FALSE(bucket_.Remove(block1));
    EXPECT_FALSE(bucket_.Remove(block2));
    EXPECT_FALSE(bucket_.Remove(block3));
    EXPECT_FALSE(bucket_.Remove(block4));
  }

  void RemovesUnspecifiedBlock() {
    std::ignore = CreateBlockAndAddToBucket(kLayout1);
    std::ignore = CreateBlockAndAddToBucket(kLayout2);
    for (size_t i = 1; i <= 2; ++i) {
      EXPECT_FALSE(bucket_.empty());
      EXPECT_NE(bucket_.RemoveAny(), nullptr);
    }
    EXPECT_TRUE(bucket_.empty());
    EXPECT_EQ(bucket_.RemoveAny(), nullptr);
  }

  void RemovesByLayout() {
    BlockType& block1 = CreateBlockAndAddToBucket(kLayout1);
    BlockType& block2 = CreateBlockAndAddToBucket(kLayout2);
    EXPECT_EQ(bucket_.RemoveCompatible(ShrinkByOne(kLayout2)), &block2);
    EXPECT_FALSE(bucket_.empty());
    EXPECT_EQ(bucket_.RemoveCompatible(ShrinkByOne(kLayout1)), &block1);
    EXPECT_TRUE(bucket_.empty());
  }

  void FailsToRemoveByExcessiveSize() {
    std::ignore = CreateBlockAndAddToBucket(kLayout1);
    std::ignore = CreateBlockAndAddToBucket(kLayout2);
    EXPECT_EQ(bucket_.RemoveCompatible(kLayout3), nullptr);
    EXPECT_FALSE(bucket_.empty());
    bucket_.Clear();
  }

 private:
  BucketType bucket_;
  std::array<std::byte, kCapacity> bytes_;
  std::array<BlockType*, kMaxBlocks> blocks_;
  BlockType* available_ = nullptr;
};

}  // namespace pw::allocator::test
