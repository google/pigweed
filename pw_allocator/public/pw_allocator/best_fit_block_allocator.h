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

#include "pw_allocator/block_allocator.h"
#include "pw_allocator/config.h"

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
          uint16_t kPoisonInterval = PW_ALLOCATOR_BLOCK_POISON_INTERVAL>
class BestFitBlockAllocator
    : public BlockAllocator<OffsetType, kPoisonInterval> {
 public:
  using Base = BlockAllocator<OffsetType, kPoisonInterval>;
  using BlockType = typename Base::BlockType;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr BestFitBlockAllocator() : Base() {}

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be large enough to fit an
  ///                     aligned block with overhead. It MUST NOT be larger
  ///                     than what is addressable by `OffsetType`.
  explicit BestFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    BlockType* best = nullptr;
    size_t best_size = std::numeric_limits<size_t>::max();
    for (auto* block : Base::blocks()) {
      size_t inner_size = block->InnerSize();
      if (block->IsFree() && inner_size < best_size &&
          block->CanAlloc(layout).ok()) {
        best = block;
        best_size = inner_size;
      }
    }
    if (best != nullptr) {
      BlockResult result = BlockType::AllocFirst(best, layout);
      PW_ASSERT(result.ok());
    }
    return best;
  }
};

}  // namespace pw::allocator
