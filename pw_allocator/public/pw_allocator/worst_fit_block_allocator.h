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

/// Block allocator that uses a "worst-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by looking at
/// all unused blocks and finding the biggest one which can satisfy the
/// request.
///
/// This algorithm may lead to less fragmentation as any unused fragments are
/// more likely to be large enough to be useful to other requests.
template <typename OffsetType = uintptr_t,
          size_t kPoisonInterval = 0,
          size_t kAlign = alignof(OffsetType)>
class WorstFitBlockAllocator
    : public BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
  using BlockType = typename Base::BlockType;

  constexpr WorstFitBlockAllocator() : Base() {}
  explicit WorstFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    // Search backwards for the biggest block that can hold this allocation.
    BlockType* worst = nullptr;
    for (auto* block : Base::rblocks()) {
      if (!block->CanAllocLast(layout.size(), layout.alignment()).ok()) {
        continue;
      }
      if (worst == nullptr || block->OuterSize() > worst->OuterSize()) {
        worst = block;
      }
    }
    if (worst != nullptr &&
        BlockType::AllocLast(worst, layout.size(), layout.alignment()).ok()) {
      return worst;
    }
    return nullptr;
  }
};

}  // namespace pw::allocator
