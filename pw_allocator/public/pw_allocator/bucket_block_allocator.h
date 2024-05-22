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

#include <array>

#include "pw_allocator/block_allocator_base.h"
#include "pw_allocator/bucket.h"
#include "pw_status/try.h"

namespace pw::allocator {

/// Block allocator that uses sized buckets of free blocks..
///
/// In this strategy, the allocator handles an allocation request by starting
/// with the bucket with the smallest size that is larger than the requested
/// size. It tries to allocate using the blocks in that block, if any, before
/// trying the bucket with the next largest size.
///
/// On deallocation, blocks are placed in the bucket of the smallest size that
/// is larger than usable space of the block being freed.
///
/// The last bucket always has an unbounded size.
///
/// As an example, assume that the allocator is configured with a minimum chunk
/// size of 64 and 5 buckets. The internal state may look like the following:
///
/// @code{.unparsed}
/// bucket[0] (64B) --> chunk[12B] --> chunk[42B] --> chunk[64B] --> NULL
/// bucket[1] (128B) --> chunk[65B] --> chunk[72B] --> NULL
/// bucket[2] (256B) --> NULL
/// bucket[3] (512B) --> chunk[312B] --> chunk[512B] --> chunk[416B] --> NULL
/// bucket[4] (implicit) --> chunk[1024B] --> chunk[513B] --> NULL
/// @endcode
template <typename OffsetType = uintptr_t,
          size_t kMinChunkSize = 32,
          size_t kNumBuckets = 5,
          size_t kPoisonInterval = 0,
          size_t kAlign = std::max(alignof(OffsetType), alignof(std::byte*))>
class BucketBlockAllocator
    : public BlockAllocator<OffsetType,
                            kPoisonInterval,
                            std::max(kAlign, alignof(std::byte*))> {
 public:
  using Base = BlockAllocator<OffsetType,
                              kPoisonInterval,
                              std::max(kAlign, alignof(std::byte*))>;
  using BlockType = typename Base::BlockType;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr BucketBlockAllocator() : Base() {
    internal::Bucket::Init(span(buckets_.data(), buckets_.size() - 1),
                           kMinChunkSize);
  }

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be large enough to fit an
  ///                     aligned block with overhead. It MUST NOT be larger
  ///                     than what is addressable by `OffsetType`.
  explicit BucketBlockAllocator(ByteSpan region) : BucketBlockAllocator() {
    Base::Init(region);
  }

  /// @copydoc BlockAllocator::Init
  void Init(ByteSpan region) { Base::Init(region); }

  /// @copydoc BlockAllocator::Init
  void Init(BlockType* begin) { Base::Init(begin); }

  /// @copydoc BlockAllocator::Init
  void Init(BlockType* begin, BlockType* end) override {
    Base::Init(begin, end);
    for (auto* block : Base::blocks()) {
      if (!block->Used()) {
        RecycleBlock(block);
      }
    }
  }

 private:
  /// @copydoc BlockAllocator::ChooseBlock
  BlockType* ChooseBlock(Layout layout) override {
    BlockType* block = nullptr;
    for (auto& bucket : buckets_) {
      if (bucket.chunk_size() < layout.size()) {
        continue;
      }
      void* leading = bucket.RemoveIf([&layout](void* chunk) {
        BlockType* candidate = BlockType::FromUsableSpace(chunk);
        return BlockType::AllocLast(candidate, layout).ok();
      });
      if (leading != nullptr) {
        block = BlockType::FromUsableSpace(leading);
        break;
      }
    }
    if (block == nullptr) {
      return nullptr;
    }
    // If the chunk was split, what we have is the leading free block.
    if (!block->Used()) {
      RecycleBlock(block);
      block = block->Next();
    }
    return block;
  }

  /// @copydoc BlockAllocator::RecycleBlock
  void RecycleBlock(BlockType* block) override {
    // Free blocks that are too small to be added to buckets will be "garbage
    // collected" by merging them with their neighbors when the latter are
    // freed.
    size_t inner_size = block->InnerSize();
    if (inner_size < sizeof(void*)) {
      return;
    }
    for (auto& bucket : buckets_) {
      if (inner_size <= bucket.chunk_size()) {
        bucket.Add(block->UsableSpace());
        break;
      }
    }
  }

  std::array<internal::Bucket, kNumBuckets> buckets_;
};

}  // namespace pw::allocator
