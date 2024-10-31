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

/// Utility function that returns the offset from an addres a given number of
/// bytes `after` a given `ptr` to the next address that has a given
/// `alignment`.
///
/// In other words, if offset is `GetAlignedOffsetAfter(ptr, alignment, after)`,
/// then `((uintptr_t)ptr + after + offset) % alignment` is 0.
///
/// This is useful when dealing with blocks that need their usable space to be
/// aligned, e.g.
///   GetAlignedOffsetAfter(bytes_.data(), layout.alignment(), kBlockOverhead);
inline size_t GetAlignedOffsetAfter(const void* ptr,
                                    size_t alignment,
                                    size_t after) {
  auto addr = reinterpret_cast<uintptr_t>(ptr) + after;
  return pw::AlignUp(addr, alignment) - addr;
}

/// Returns the minimum outer size for a block allocated from a layout with the
/// given `min_inner_size`.
template <typename BlockType>
size_t GetOuterSize(size_t min_inner_size) {
  return BlockType::kBlockOverhead +
         pw::AlignUp(min_inner_size, BlockType::kAlignment);
}

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
                       std::initializer_list<Preallocation> preallocs) {
  // First, look if any blocks use kSizeRemaining, and calculate how large
  // that will be.
  bytes = GetAlignedSubspan(bytes, BlockType::kAlignment);
  size_t remaining_outer_size = bytes.size();
  for (auto& preallocation : preallocs) {
    if (preallocation.outer_size != Preallocation::kSizeRemaining) {
      size_t outer_size =
          AlignUp(preallocation.outer_size, BlockType::kAlignment);
      PW_ASSERT(remaining_outer_size >= outer_size);
      remaining_outer_size -= outer_size;
    }
  }

  // Now, construct objects in place.
  BlockType* block = nullptr;
  size_t roffset = bytes.size();
  for (auto it = std::rbegin(preallocs); it != std::rend(preallocs); ++it) {
    const Preallocation& preallocation = *it;
    size_t outer_size = preallocation.outer_size;
    if (outer_size == Preallocation::kSizeRemaining) {
      outer_size = remaining_outer_size;
      remaining_outer_size = 0;
    } else {
      outer_size = AlignUp(preallocation.outer_size, BlockType::kAlignment);
    }
    PW_CHECK_SUB(roffset, outer_size, &roffset);
    ByteSpan region = bytes.subspan(roffset, outer_size);
    BlockType* next = block;
    block = *(BlockType::Init(region, next));
    if (preallocation.state != Preallocation::kFree) {
      auto result = BlockType::AllocLast(block, Layout(block->InnerSize(), 1));
      PW_CHECK_OK(result.status());
      PW_CHECK_UINT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
      PW_CHECK_UINT_EQ(result.next(), BlockResult::Next::kUnchanged);
    }
  }
  return block;
}

}  // namespace pw::allocator::test
