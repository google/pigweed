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

#include "pw_allocator/block/alignable.h"
#include "pw_allocator/layout.h"
#include "pw_assert/assert.h"

namespace pw::allocator {
namespace internal {

// Trivial base class for trait support.
struct BaseWithLayout {};

}  // namespace internal

/// Mix-in for blocks that can retrieve the layout used to allocate them.
///
/// Block mix-ins are stateless and trivially constructible. See `BasicBlock`
/// for details on how mix-ins can be combined to implement blocks.
///
/// This mix-in requires its derived type also derive from `AlignableBlock`
/// and provide the following symbols:
///
/// - size_t RequestedSize() const
///   - Returns the size of the original layout
/// - size_t RequestedAlignment() const
///   - Returns the alignment of the original layout
/// - void SetRequestedSize(size_t)
///   - Records the size of the original layout
/// - void SetRequestedAlignment(size_t)
///   - Records the alignment from the original layout
template <typename Derived>
class BlockWithLayout : public internal::BaseWithLayout {
 protected:
  constexpr explicit BlockWithLayout() {
    // Assert within a function, since `Derived` is not complete when this type
    // is defined.
    static_assert(is_alignable_v<Derived>,
                  "Types derived from BlockWithLayout must also derive from "
                  "AlignableBlock");
  }

 public:
  /// @returns The memory layout that was requested using AllocFirst, AllocLast,
  /// or Resize, or FAILED_PRECONDITION if the block is free.
  Result<Layout> RequestedLayout() const;

 protected:
  /// @copydoc AllocatableBlock::AllocFirst
  static BlockResult DoAllocFirst(Derived*& block, Layout layout);

  /// @copydoc AllocatableBlock::AllocLast
  static BlockResult DoAllocLast(Derived*& block, Layout layout);

  /// @copydoc AllocatableBlock::Resize
  static BlockResult DoResize(Derived*& block,
                              size_t new_inner_size,
                              bool shifted = false);

  /// @copydoc AllocatableBlock::Free
  static void DoFree(Derived*& block);

 private:
  constexpr const Derived* derived() const {
    return static_cast<const Derived*>(this);
  }
};

/// Trait type that allow interrogating a block as to whether it records the
/// requested layout.
template <typename BlockType>
struct has_layout : std::is_base_of<internal::BaseWithLayout, BlockType> {};

/// Helper variable template for `has_layout<BlockType>::value`.
template <typename BlockType>
inline constexpr bool has_layout_v = has_layout<BlockType>::value;

// Template method implementations.

template <typename Derived>
Result<Layout> BlockWithLayout<Derived>::RequestedLayout() const {
  derived()->CheckInvariantsIfStrict();
  if (derived()->IsFree()) {
    return Status::FailedPrecondition();
  }
  return Layout(derived()->RequestedSize(), derived()->RequestedAlignment());
}

template <typename Derived>
BlockResult BlockWithLayout<Derived>::DoAllocFirst(Derived*& block,
                                                   Layout layout) {
  BlockResult result = AlignableBlock<Derived>::DoAllocFirst(block, layout);
  if (result.ok()) {
    block->SetRequestedSize(layout.size());
    block->SetRequestedAlignment(layout.alignment());
  }
  return result;
}

template <typename Derived>
BlockResult BlockWithLayout<Derived>::DoAllocLast(Derived*& block,
                                                  Layout layout) {
  BlockResult result = AlignableBlock<Derived>::DoAllocLast(block, layout);
  if (result.ok()) {
    block->SetRequestedSize(layout.size());
    block->SetRequestedAlignment(layout.alignment());
  }
  return result;
}

template <typename Derived>
BlockResult BlockWithLayout<Derived>::DoResize(Derived*& block,
                                               size_t new_inner_size,
                                               bool shifted) {
  size_t old_size = block->RequestedSize();
  BlockResult result =
      AllocatableBlock<Derived>::DoResize(block, new_inner_size, shifted);
  if (result.ok() && !shifted) {
    block->SetRequestedSize(new_inner_size);
  } else {
    block->SetRequestedSize(old_size);
  }
  return result;
}

template <typename Derived>
void BlockWithLayout<Derived>::DoFree(Derived*& block) {
  AllocatableBlock<Derived>::DoFree(block);
  Derived* prev = block->Prev();
  if (prev == nullptr) {
    return;
  }
  size_t prev_size = prev->RequestedSize();
  if (prev->InnerSize() - prev_size < Derived::kAlignment) {
    return;
  }
  // Reclaim bytes that were shifted to prev when the block allocated.
  AllocatableBlock<Derived>::DoResize(prev, prev_size, true)
      .IgnoreUnlessStrict();
  block = prev->Next();
}

}  // namespace pw::allocator
