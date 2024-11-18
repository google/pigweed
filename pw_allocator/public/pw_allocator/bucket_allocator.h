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
#include <cstdint>

#include "pw_allocator/block/detailed_block.h"
#include "pw_allocator/block_allocator.h"
#include "pw_allocator/bucket.h"
#include "pw_assert/check.h"
#include "pw_status/try.h"

namespace pw::allocator {

/// Block allocator that uses sized buckets of free blocks.
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
///
/// Note that since this allocator stores information in free chunks, it does
/// not currently support poisoning.
template <size_t kMinBucketChunkSize = 32, size_t kNumBuckets = 5>
class BucketAllocator : public BlockAllocator<uintptr_t, 0> {
 public:
  using Base = BlockAllocator<uintptr_t, 0>;
  using BlockType = typename Base::BlockType;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr BucketAllocator() : Base() {}

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be large enough to fit an
  ///                     aligned block with overhead. It MUST NOT be larger
  ///                     than what is addressable by `OffsetType`.
  explicit BucketAllocator(ByteSpan region) : BucketAllocator() {
    Base::Init(region);
  }

  /// @copydoc BlockAllocator::Init
  void Init(ByteSpan region) { Base::Init(region); }

  /// @copydoc BlockAllocator::Init
  void Init(BlockType* begin) { Base::Init(begin); }

  /// @copydoc BlockAllocator::Init
  void Init(BlockType* begin, BlockType* end) override {
    Base::Init(begin, end);
    internal::Bucket::Init(span(buckets_.data(), buckets_.size() - 1),
                           kMinBucketChunkSize);
    buckets_.back().Init();
    for (auto* block : Base::blocks()) {
      if (block->IsFree()) {
        RecycleBlock(block);
      }
    }
  }

 private:
  /// @copydoc BlockAllocator::ChooseBlock
  BlockResult<BlockType> ChooseBlock(Layout layout) override {
    layout =
        Layout(std::max(layout.size(), sizeof(internal::Bucket::Chunk)),
               std::max(layout.alignment(), alignof(internal::Bucket::Chunk)));
    for (auto& bucket : buckets_) {
      if (bucket.chunk_size() < layout.size()) {
        continue;
      }
      void* chosen = bucket.RemoveIf([&layout](const std::byte* chunk) {
        const BlockType* block = BlockType::FromUsableSpace(chunk);
        return block->CanAlloc(layout).ok();
      });
      if (chosen == nullptr) {
        continue;
      }
      BlockType* block = BlockType::FromUsableSpace(chosen);
      return BlockType::AllocLast(std::move(block), layout);
    }
    return BlockResult<BlockType>(nullptr, Status::NotFound());
  }

  /// @copydoc BlockAllocator::ReserveBlock
  void ReserveBlock(BlockType* block) override {
    PW_ASSERT(block->IsFree());
    size_t inner_size = block->InnerSize();
    if (inner_size < sizeof(internal::Bucket::Chunk)) {
      return;
    }
    internal::Bucket::Remove(block->UsableSpace());
  }

  /// @copydoc BlockAllocator::RecycleBlock
  void RecycleBlock(BlockType* block) override {
    PW_ASSERT(block->IsFree());
    size_t inner_size = block->InnerSize();
    if (inner_size < sizeof(internal::Bucket::Chunk)) {
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
