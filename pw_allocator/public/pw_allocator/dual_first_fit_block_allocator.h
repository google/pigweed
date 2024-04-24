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

/// Block allocator that uses a "dual first-fit" allocation strategy split
/// between large and small allocations.
///
/// In this strategy, the strategy includes a threshold value. Requests for
/// more than this threshold are handled similarly to `FirstFit`, while requests
/// for less than this threshold are handled similarly to `LastFit`.
///
/// This algorithm approaches the performance of `FirstFit` and `LastFit` while
/// improving on those algorithms fragmentation.
template <typename OffsetType = uintptr_t,
          size_t kPoisonInterval = 0,
          size_t kAlign = alignof(OffsetType)>
class DualFirstFitBlockAllocator
    : public BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
  using BlockType = typename Base::BlockType;

  constexpr DualFirstFitBlockAllocator() : Base() {}
  DualFirstFitBlockAllocator(ByteSpan region, size_t threshold)
      : Base(region), threshold_(threshold) {}

  /// Sets the threshold value for which requests are considered "large".
  void set_threshold(size_t threshold) { threshold_ = threshold; }

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    if (layout.size() < threshold_) {
      // Search backwards for the last block that can hold this allocation.
      for (auto* block : Base::rblocks()) {
        if (BlockType::AllocLast(block, layout.size(), layout.alignment())
                .ok()) {
          return block;
        }
      }
    } else {
      // Search forwards for the first block that can hold this allocation.
      for (auto* block : Base::blocks()) {
        if (BlockType::AllocFirst(block, layout.size(), layout.alignment())
                .ok()) {
          return block;
        }
      }
    }
    // No valid block found.
    return nullptr;
  }

  size_t threshold_ = 0;
};

}  // namespace pw::allocator
