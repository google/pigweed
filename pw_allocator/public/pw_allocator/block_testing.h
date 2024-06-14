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

#include "pw_allocator/block.h"
#include "pw_assert/assert.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"

namespace pw::allocator::test {

/// Represents an initial state for a memory block.
///
/// Unit tests can specify an initial block layout by passing a list of these
/// structs to `Preallocate`.
///
/// The outer size of each block must be more than `kBlockOverhead` for the
/// block type in use. The special `kSizeRemaining` may be used for at most
/// one block to give it any space not assigned to other blocks.
///
/// The state must be either `kFree` or `kUsed`.
///
/// Example:
/// @code{.cpp}
///   // BlockType = UnpoisonedBlock<uint32_t>, so kBlockOverhead == 8.
///   BlockType* block1 = Preallocate({
///     {32,              kUsed},
///     {24,              kFree},
///     {48,              kUsed},
///     {kSizeRemaining,  kFree},
///     {64,              kUsed},
///   });
/// @endcode
struct Preallocation {
  /// The outer size of the block to preallocate.
  size_t outer_size;

  // Index into the `test_fixture` array where the pointer to the block's
  // space should be cached.
  enum class State {
    kUsed,
    kFree,
  } state;

  static constexpr State kUsed = State::kUsed;
  static constexpr State kFree = State::kFree;

  /// Special value indicating the block should comprise of the all remaining
  /// space not preallocated to any other block. May be used at most once.
  static constexpr size_t kSizeRemaining = std::numeric_limits<size_t>::max();
};

template <typename BlockType>
BlockType* Preallocate(ByteSpan bytes,
                       std::initializer_list<Preallocation> preallocations) {
  BlockType* first = nullptr;

  // First, look if any blocks use kSizeRemaining, and calculate how large
  // that will be.
  Result<BlockType*> result = BlockType::Init(bytes);
  BlockType* block = *result;
  size_t remaining_outer_size = block->OuterSize();
  for (auto& preallocation : preallocations) {
    if (preallocation.outer_size != Preallocation::kSizeRemaining) {
      size_t outer_size =
          AlignUp(preallocation.outer_size, BlockType::kAlignment);
      PW_ASSERT(remaining_outer_size >= outer_size);
      remaining_outer_size -= outer_size;
    }
  }

  // Allocate each block.
  for (auto& preallocation : preallocations) {
    PW_ASSERT(block != nullptr);
    size_t outer_size = preallocation.outer_size;
    if (outer_size == Preallocation::kSizeRemaining) {
      outer_size = remaining_outer_size;
      remaining_outer_size = 0;
    }
    PW_ASSERT(outer_size > BlockType::kBlockOverhead);
    Layout layout(outer_size - BlockType::kBlockOverhead, 1);
    PW_ASSERT(BlockType::AllocFirst(block, layout).ok());
    if (first == nullptr) {
      first = block;
    }
    block = block->Next();
  }

  // Now free the appropriate blocks.
  block = first;
  for (auto& preallocation : preallocations) {
    if (preallocation.state == Preallocation::kFree) {
      BlockType::Free(block);
    }
    block = block->Next();
  }

  return first;
}

}  // namespace pw::allocator::test
