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
#include "pw_allocator/config.h"
#include "pw_preprocessor/compiler.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// Alias for a default block type that is compatible with
/// `WorstFitBlockAllocator`.
template <typename OffsetType>
using WorstFitBlock = DetailedBlock<OffsetType>;

/// Block allocator that uses a "worst-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by looking at
/// all unused blocks and finding the biggest one which can satisfy the
/// request.
///
/// This algorithm may lead to less fragmentation as any unused fragments are
/// more likely to be large enough to be useful to other requests.
template <typename OffsetType = uintptr_t>
class WorstFitBlockAllocator
    : public BlockAllocator<WorstFitBlock<OffsetType>> {
 public:
  using BlockType = WorstFitBlock<OffsetType>;

 private:
  using Base = BlockAllocator<BlockType>;

 public:
  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr WorstFitBlockAllocator() = default;

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be valid as an argument to
  ///                     `BlockType::Init`.
  explicit WorstFitBlockAllocator(ByteSpan region) { Base::Init(region); }

 private:
  /// @copydoc Allocator::Allocate
  BlockResult<BlockType> ChooseBlock(Layout layout) override {
    BlockType* worst = nullptr;
    size_t worst_size = layout.size() - 1;
    for (auto* block : Base::blocks()) {
      size_t inner_size = block->InnerSize();
      if (block->IsFree() && worst_size < inner_size &&
          block->CanAlloc(layout).ok()) {
        worst = block;
        worst_size = inner_size;
      }
    }
    if (worst != nullptr) {
      return BlockType::AllocFirst(std::move(worst), layout);
    }
    return BlockResult<BlockType>(nullptr, Status::NotFound());
  }
};

}  // namespace pw::allocator
