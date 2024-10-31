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

#include "pw_allocator/block_allocator.h"
#include "pw_allocator/config.h"

namespace pw::allocator {

/// Block allocator that uses a "last-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by starting at
/// the end of the range of blocks and looking for the last one which can
/// satisfy the request.
///
/// This strategy may result in slightly better fragmentation than the
/// corresponding "first-fit" strategy, since even with alignment it will result
/// in at most one unused fragment before the allocated block.
template <typename OffsetType = uintptr_t,
          uint16_t kPoisonInterval = PW_ALLOCATOR_BLOCK_POISON_INTERVAL>
class LastFitBlockAllocator
    : public BlockAllocator<OffsetType, kPoisonInterval> {
 public:
  using Base = BlockAllocator<OffsetType, kPoisonInterval>;
  using BlockType = typename Base::BlockType;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr LastFitBlockAllocator() : Base() {}

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be large enough to fit an
  ///                     aligned block with overhead. It MUST NOT be larger
  ///                     than what is addressable by `OffsetType`.
  explicit LastFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    // Search backwards for the last block that can hold this allocation.
    for (auto* block : Base::rblocks()) {
      if (BlockType::AllocLast(block, layout).ok()) {
        return block;
      }
    }
    return nullptr;
  }
};

}  // namespace pw::allocator
