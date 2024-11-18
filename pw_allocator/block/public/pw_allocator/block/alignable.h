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
#include <cstdint>

#include "lib/stdcompat/bit.h"
#include "pw_allocator/block/allocatable.h"
#include "pw_allocator/block/result.h"
#include "pw_allocator/layout.h"
#include "pw_assert/assert.h"
#include "pw_bytes/alignment.h"
#include "pw_status/status.h"

namespace pw::allocator {
namespace internal {

// Trivial base class for trait support.
struct AlignableBase {};

}  // namespace internal

/// Mix-in for blocks that can be split on alignment boundaries.
///
/// Block mix-ins are stateless and trivially constructible. See `BasicBlock`
/// for details on how mix-ins can be combined to implement blocks.
///
/// This mix-in requires its derived type also derive from `AllocatableBlock`.
template <typename Derived>
class AlignableBlock : public internal::AlignableBase {
 protected:
  constexpr explicit AlignableBlock() {
    // Assert within a function, since `Derived` is not complete when this type
    // is defined.
    static_assert(is_allocatable_v<Derived>,
                  "Types derived from AlignableBlock must also derive from "
                  "AllocatableBlock");
  }

  /// @copydoc AllocatableBlock::CanAlloc
  static StatusWithSize DoCanAlloc(const Derived* block, Layout layout);

  /// @copydoc AllocatableBlock::AllocFirst
  static BlockResult DoAllocFirst(Derived*& block, Layout layout);

  /// @copydoc AllocatableBlock::AllocLast
  static BlockResult DoAllocLast(Derived*& block, Layout layout);

 private:
  /// Allocates a block of `new_inner_size` that starts after `leading` bytes.
  static BlockResult DoAllocAligned(Derived*& block,
                                    size_t leading,
                                    size_t new_inner_size);

  // BlockWithLayout calls DoAllocFirst
  template <typename>
  friend class BlockWithLayout;
};

/// Trait type that allows interrogating a block as to whether it is alignable.
template <typename BlockType>
struct is_alignable : std::is_base_of<internal::AlignableBase, BlockType> {};

/// Helper variable template for `is_alignable<BlockType>::value`.
template <typename BlockType>
inline constexpr bool is_alignable_v = is_alignable<BlockType>::value;

// Template method implementations.

template <typename Derived>
StatusWithSize AlignableBlock<Derived>::DoCanAlloc(const Derived* block,
                                                   Layout layout) {
  // How much extra space is available?
  auto result = AllocatableBlock<Derived>::DoCanAlloc(block, layout);
  if (!result.ok()) {
    return StatusWithSize(result.status(), 0);
  }
  size_t extra = result.size();

  // Is the default alignment sufficient?
  if (layout.alignment() <= Derived::kAlignment) {
    return StatusWithSize(extra);
  }

  // What is the last aligned address within the leading extra space?
  auto addr = cpp20::bit_cast<uintptr_t>(block->UsableSpace());
  uintptr_t aligned_addr;
  PW_ASSERT(!PW_ADD_OVERFLOW(addr, extra, &aligned_addr));
  aligned_addr = AlignDown(aligned_addr, layout.alignment());

  // Is there an aligned address within the extra space?
  if (aligned_addr < addr) {
    return StatusWithSize::ResourceExhausted();
  }

  // If splitting the first block, is there enough extra for a valid block?
  size_t leading_outer_size = aligned_addr - addr;
  if (leading_outer_size != 0 && leading_outer_size < Derived::kMinOuterSize &&
      block->Prev() == nullptr) {
    return StatusWithSize::ResourceExhausted();
  }

  return StatusWithSize(leading_outer_size);
}

template <typename Derived>
BlockResult AlignableBlock<Derived>::DoAllocFirst(Derived*& block,
                                                  Layout layout) {
  // Is the default alignment sufficient?
  if (layout.alignment() <= Derived::kAlignment) {
    return AllocatableBlock<Derived>::DoAllocFirst(block, layout);
  }

  // What is the first aligned address within the leading extra space?
  size_t size = AlignUp(layout.size(), Derived::kAlignment);
  layout = Layout(size, layout.alignment());
  StatusWithSize can_alloc = Derived::DoCanAlloc(block, layout);
  if (!can_alloc.ok()) {
    return BlockResult(can_alloc.status());
  }
  size_t extra = can_alloc.size();
  size_t leading_outer_size = extra - AlignDown(extra, layout.alignment());

  // If splitting the first block, there must be enough for a valid block.
  if (leading_outer_size != 0 && leading_outer_size <= Derived::kMinOuterSize &&
      block->Prev() == nullptr) {
    leading_outer_size += AlignUp(Derived::kMinOuterSize - leading_outer_size,
                                  layout.alignment());
  }
  if (leading_outer_size > extra) {
    return BlockResult(Status::ResourceExhausted());
  }

  // Allocate the aligned block.
  return DoAllocAligned(block, leading_outer_size, layout.size());
}

template <typename Derived>
BlockResult AlignableBlock<Derived>::DoAllocLast(Derived*& block,
                                                 Layout layout) {
  // Is the default alignment sufficient?
  if (layout.alignment() <= Derived::kAlignment) {
    return AllocatableBlock<Derived>::DoAllocLast(block, layout);
  }

  // What is the last aligned address within the leading extra space?
  size_t size = AlignUp(layout.size(), Derived::kAlignment);
  layout = Layout(size, layout.alignment());
  StatusWithSize can_alloc = Derived::DoCanAlloc(block, layout);
  if (!can_alloc.ok()) {
    return BlockResult(can_alloc.status());
  }
  size_t extra = can_alloc.size();

  // Allocate the aligned block.
  return DoAllocAligned(block, extra, layout.size());
}

template <typename Derived>
BlockResult AlignableBlock<Derived>::DoAllocAligned(Derived*& block,
                                                    size_t leading_outer_size,
                                                    size_t new_inner_size) {
  // Allocate everything after aligned address.
  Layout layout(block->InnerSize() - leading_outer_size, Derived::kAlignment);
  BlockResult alloc_result =
      AllocatableBlock<Derived>::DoAllocLast(block, layout);
  if (!alloc_result.ok()) {
    return alloc_result;
  }

  // Resize the allocation to the requested size.
  BlockResult resize_result =
      AllocatableBlock<Derived>::DoResize(block, new_inner_size);
  if (!resize_result.ok()) {
    return resize_result;
  }

  return BlockResult(alloc_result.prev(), resize_result.next());
}

}  // namespace pw::allocator
