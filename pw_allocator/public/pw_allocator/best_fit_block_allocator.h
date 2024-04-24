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

#include "pw_allocator/block_allocator_base.h"

namespace pw::allocator {

/// Block allocator that uses a "best-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by looking at
/// all unused blocks and finding the smallest one which can satisfy the
/// request.
///
/// This algorithm may make better use of available memory by wasting less on
/// unused fragments, but may also lead to worse fragmentation as those
/// fragments are more likely to be too small to be useful to other requests.
template <typename OffsetType = uintptr_t,
          size_t kPoisonInterval = 0,
          size_t kAlign = alignof(OffsetType)>
class BestFitBlockAllocator
    : public BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
  using BlockType = typename Base::BlockType;

  constexpr BestFitBlockAllocator() : Base() {}
  explicit BestFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    // Search backwards for the smallest block that can hold this allocation.
    BlockType* best = nullptr;
    for (auto* block : Base::rblocks()) {
      if (!block->CanAllocLast(layout.size(), layout.alignment()).ok()) {
        continue;
      }
      if (best == nullptr || block->OuterSize() < best->OuterSize()) {
        best = block;
      }
    }
    if (best != nullptr &&
        BlockType::AllocLast(best, layout.size(), layout.alignment()).ok()) {
      return best;
    }
    return nullptr;
  }
};

}  // namespace pw::allocator
