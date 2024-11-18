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

#include "pw_allocator/block/detailed_block.h"
#include "pw_allocator/block_allocator.h"

namespace pw::allocator {

/// Alias for a default block type that is compatible with
/// `FirstFitBlockAllocator`.
template <typename OffsetType>
using FirstFitBlock = DetailedBlock<OffsetType>;

/// Block allocator that uses a "first-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by starting at
/// the beginning of the range of blocks and looking for the first one which can
/// satisfy the request.
///
/// This strategy may result in slightly worse fragmentation than the
/// corresponding "last-fit" strategy, since the alignment may result in unused
/// fragments both before and after an allocated block.
template <typename OffsetType = uintptr_t>
class FirstFitBlockAllocator
    : public BlockAllocator<FirstFitBlock<OffsetType>> {
 public:
  using BlockType = FirstFitBlock<OffsetType>;

 private:
  using Base = BlockAllocator<BlockType>;

 public:
  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr FirstFitBlockAllocator() = default;

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be valid as an argument to
  ///                     `BlockType::Init`.
  explicit FirstFitBlockAllocator(ByteSpan region) { Base::Init(region); }

 private:
  /// @copydoc Allocator::Allocate
  BlockResult<BlockType> ChooseBlock(Layout layout) override {
    // Search forwards for the first block that can hold this allocation.
    for (auto* block : Base::blocks()) {
      auto result = BlockType::AllocFirst(std::move(block), layout);
      if (result.ok()) {
        return result;
      }
    }
    return BlockResult<BlockType>(nullptr, Status::NotFound());
  }
};

}  // namespace pw::allocator
