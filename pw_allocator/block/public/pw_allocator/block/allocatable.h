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

#include "pw_allocator/block/contiguous.h"
#include "pw_allocator/block/result.h"
#include "pw_allocator/layout.h"
#include "pw_assert/assert.h"
#include "pw_bytes/alignment.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::allocator {
namespace internal {

// Trivial base class for trait support.
struct AllocatableBase {};

}  // namespace internal

/// Mix-in for blocks that can be allocated and freed.
///
/// Block mix-ins are stateless and trivially constructible. See `BasicBlock`
/// for details on how mix-ins can be combined to implement blocks.
///
/// This mix-in requires its derived type also derive from `ContiguousBlock`
/// and provide the following symbols:
///
/// - size_t kMinOuterSize
///   - Size of the smallest block that can be allocated.
/// - bool IsFreeUnchecked() const
///   - Returns whether the block is free or in use.
/// - void SetFree(bool)
///   - Sets whether the block is free or in use.
template <typename Derived>
class AllocatableBlock : public internal::AllocatableBase {
 protected:
  constexpr explicit AllocatableBlock() {
    // Assert within a function, since `Derived` is not complete when this type
    // is defined.
    static_assert(is_contiguous_v<Derived>,
                  "Types derived from AllocatableBlock must also derive from "
                  "ContiguousBlock");
  }

 public:
  /// @returns whether this block is free or is in use.
  inline bool IsFree() const;

  /// Indicates whether the block is in use is free.
  ///
  /// This method will eventually be deprecated. Prefer `IsFree`.
  inline bool Used() const { return !IsFree(); }

  /// Checks if a block could be split from the block.
  ///
  /// On error, this method will return the same status as `AllocFirst` or
  /// `AllocLast` without performing any modifications.
  ///
  /// @pre The block must not be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Returns the number of bytes to shift this block in order to align
  ///    its usable space.
  ///
  ///    FAILED_PRECONDITION: This block is in use and cannot be split.
  ///
  ///    RESOURCE_EXHAUSTED: The available space is insufficient to fulfill the
  ///    request. This may be due to a large requested size, or insufficient
  ///    remaining space to fulfill the requested alignment create a valid
  ///    leading block, and/or create a valid trailing block.
  ///
  /// @endrst
  StatusWithSize CanAlloc(Layout layout) const;

  /// Splits an aligned block from the start of the block, and marks it as used.
  ///
  /// If successful, `block` will be replaced by a block that has an inner
  /// size of at least `inner_size`, and whose starting address is aligned to an
  /// `alignment` boundary. If unsuccessful, `block` will be unmodified.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block. In total, up to two
  /// additional blocks may be created: one to pad the returned block to an
  /// alignment boundary and one for the trailing space.
  ///
  /// For larger alignments, the `AllocLast` method is generally preferable to
  /// this method, as this method may create an additional fragments both before
  /// and after the returned block in order to align the usable space.
  ///
  /// @pre The block must not be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The split completed successfully. The `BlockAllocType` indicates
  ///    how extra memory was distributed to other blocks.
  ///
  ///    FAILED_PRECONDITION: This block is in use and cannot be split.
  ///
  ///    RESOURCE_EXHAUSTED: The available space is insufficient to fulfill the
  ///    request. This may be due to a large requested size, or insufficient
  ///    remaining space to fulfill the requested alignment create a valid
  ///    leading block, and/or create a valid trailing block.
  ///
  /// @endrst
  static BlockResult AllocFirst(Derived*& block, Layout layout);

  /// Splits an aligned block from the end of the block, and marks it as used.
  ///
  /// If successful, `block` will be replaced by a block that has an inner
  /// size of at least `inner_size`, and whose starting address is aligned to an
  /// `alignment` boundary. If unsuccessful, `block` will be unmodified.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block. An additional block may
  /// be created for the leading space.
  ///
  /// This method may create an additional fragment before the returned block
  /// in order to align the usable space.
  ///
  /// @pre The block must not be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The split completed successfully. The `BlockAllocType` indicates
  ///    how extra memory was distributed to other blocks.
  ///
  ///    FAILED_PRECONDITION: This block is in use and cannot be split.
  ///
  ///    RESOURCE_EXHAUSTED: The available space is insufficient to fulfill the
  ///    request. This may be due to a large requested size, or insufficient
  ///    remaining space to fulfill the requested alignment create a valid
  ///    leading block, and/or create a valid trailing block.
  ///
  /// @endrst
  static BlockResult AllocLast(Derived*& block, Layout layout);

  /// Grows or shrinks the block.
  ///
  /// If successful, `block` may be merged with the block after it in order to
  /// provide additional memory (when growing) or to merge released memory (when
  /// shrinking). If unsuccessful, `block` will be unmodified.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer with a pointer to the new, smaller block.
  ///
  /// @pre The block must be in use.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The resize completed successfully.
  ///
  ///    FAILED_PRECONDITION: This block is not in use.
  ///
  ///    RESOURCE_EXHAUSTED: The available space is insufficient to fulfill the
  ///    request. This may be due to a large requested size, or insufficient
  ///    remaining space to fulfill the requested alignment create a valid
  ///    leading block, and/or create a valid trailing block.
  ///
  /// @endrst
  static BlockResult Resize(Derived*& block, size_t new_inner_size);

  /// Marks the block as free.
  ///
  /// This method is static in order to consume and replace the given block
  /// pointer. If neither member is free, the returned pointer will point to the
  /// original block. Otherwise, it will point to the new, larger block created
  /// by merging adjacent free blocks together.
  static void Free(Derived*& block);

 protected:
  /// @copydoc CanAlloc
  static StatusWithSize DoCanAlloc(const Derived* block, Layout layout);

  /// @copydoc AllocFirst
  static BlockResult DoAllocFirst(Derived*& block, Layout layout);

  /// @copydoc AllocLast
  static BlockResult DoAllocLast(Derived*& block, Layout layout);

  /// @copydoc Resize
  static BlockResult DoResize(Derived*& block,
                              size_t new_inner_size,
                              bool shifted = false);

  /// @copydoc Free
  static void DoFree(Derived*& block);

 private:
  constexpr const Derived* derived() const {
    return static_cast<const Derived*>(this);
  }

  // AlignableBlock calls DoCanAlloc, DoAllocLast, DoResize
  template <typename>
  friend class AlignableBlock;

  // BlockWithLayout calls DoFree, DoResize
  template <typename>
  friend class BlockWithLayout;
};

/// Trait type that allows interrogating a block as to whether it is
/// allocatable.
template <typename BlockType>
struct is_allocatable : std::is_base_of<internal::AllocatableBase, BlockType> {
};

/// Helper variable template for `is_allocatable<BlockType>::value`.
template <typename BlockType>
inline constexpr bool is_allocatable_v = is_allocatable<BlockType>::value;

// Template method implementations.

template <typename Derived>
bool AllocatableBlock<Derived>::IsFree() const {
  derived()->CheckInvariantsIfStrict();
  return derived()->IsFreeUnchecked();
}

template <typename Derived>
StatusWithSize AllocatableBlock<Derived>::CanAlloc(Layout layout) const {
  derived()->CheckInvariantsIfStrict();
  return Derived::DoCanAlloc(derived(), layout);
}

template <typename Derived>
StatusWithSize AllocatableBlock<Derived>::DoCanAlloc(const Derived* block,
                                                     Layout layout) {
  if (!block->IsFree()) {
    return StatusWithSize::FailedPrecondition();
  }
  if (layout.size() == 0) {
    return StatusWithSize::InvalidArgument();
  }
  size_t extra;
  size_t new_inner_size = AlignUp(layout.size(), Derived::kAlignment);
  if (PW_SUB_OVERFLOW(block->InnerSize(), new_inner_size, &extra)) {
    return StatusWithSize::ResourceExhausted();
  }
  return StatusWithSize(extra);
}

template <typename Derived>
BlockResult AllocatableBlock<Derived>::AllocFirst(Derived*& block,
                                                  Layout layout) {
  if (block == nullptr || layout.size() == 0) {
    return BlockResult(Status::InvalidArgument());
  }
  block->CheckInvariants(/* crash_on_failure: */ true);
  if (!block->IsFree()) {
    return BlockResult(Status::FailedPrecondition());
  }
  BlockResult result = Derived::DoAllocFirst(block, layout);
  block->CheckInvariantsIfStrict();
  return BlockResult(result);
}

template <typename Derived>
BlockResult AllocatableBlock<Derived>::DoAllocFirst(Derived*& block,
                                                    Layout layout) {
  size_t size = AlignUp(layout.size(), Derived::kAlignment);
  layout = Layout(size, layout.alignment());
  StatusWithSize can_alloc = Derived::DoCanAlloc(block, layout);
  if (!can_alloc.ok()) {
    return BlockResult(can_alloc.status());
  }
  size_t extra = can_alloc.size();
  BlockResult::Next result = BlockResult::Next::kUnchanged;
  if (extra >= Derived::kMinOuterSize) {
    // Split the large padding off the back.
    Derived::DoSplitFirst(block, block->InnerSize() - extra);
    result = BlockResult::Next::kSplitNew;
  }
  block->SetFree(false);
  return BlockResult(result);
}

template <typename Derived>
BlockResult AllocatableBlock<Derived>::AllocLast(Derived*& block,
                                                 Layout layout) {
  if (block == nullptr || layout.size() == 0) {
    return BlockResult(Status::InvalidArgument());
  }
  block->CheckInvariants(/* crash_on_failure: */ true);
  if (!block->IsFree()) {
    return BlockResult(Status::FailedPrecondition());
  }
  BlockResult result = Derived::DoAllocLast(block, layout);
  block->CheckInvariantsIfStrict();
  return result;
}

template <typename Derived>
BlockResult AllocatableBlock<Derived>::DoAllocLast(Derived*& block,
                                                   Layout layout) {
  size_t size = AlignUp(layout.size(), Derived::kAlignment);
  layout = Layout(size, layout.alignment());
  StatusWithSize can_alloc = Derived::DoCanAlloc(block, layout);
  if (!can_alloc.ok()) {
    return BlockResult(can_alloc.status());
  }
  size_t extra = can_alloc.size();
  BlockResult::Prev result = BlockResult::Prev::kUnchanged;
  Derived* prev = block->Prev();
  if (extra >= Derived::kMinOuterSize) {
    // Split the large padding off the front.
    Derived::DoSplitLast(block, layout.size());
    result = BlockResult::Prev::kSplitNew;

  } else if (extra != 0 && prev != nullptr) {
    // The small amount of padding can be appended to the previous block.
    Derived::DoResize(prev, prev->InnerSize() + extra, true)
        .IgnoreUnlessStrict();
    block = prev->Next();
    result = BlockResult::Prev::kResized;
  }
  block->SetFree(false);
  return BlockResult(result);
}

template <typename Derived>
BlockResult AllocatableBlock<Derived>::Resize(Derived*& block,
                                              size_t new_inner_size) {
  if (block == nullptr) {
    return BlockResult(Status::InvalidArgument());
  }
  block->CheckInvariants(/* crash_on_failure: */ true);
  if (block->IsFree()) {
    return BlockResult(Status::FailedPrecondition());
  }
  BlockResult result =
      Derived::DoResize(block, new_inner_size, /* shifted: */ false);
  block->CheckInvariantsIfStrict();
  return result;
}

template <typename Derived>
BlockResult AllocatableBlock<Derived>::DoResize(Derived*& block,
                                                size_t new_inner_size,
                                                bool) {
  size_t old_inner_size = block->InnerSize();
  new_inner_size = AlignUp(new_inner_size, Derived::kAlignment);
  if (old_inner_size == new_inner_size) {
    return BlockResult();
  }

  // Treat the block as free and try to combine it with the next block. At most
  // one free block is expected to follow this block.
  BlockResult::Next result = BlockResult::Next::kUnchanged;
  block->SetFree(true);
  Derived* next = block->Next();
  if (next != nullptr && next->IsFree()) {
    Derived::DoMergeNext(block);
    result = BlockResult::Next::kMerged;
  }
  if (block->InnerSize() < new_inner_size) {
    // The merged block is too small for the resized block. Restore the original
    // blocks as needed.
    if (block->InnerSize() != old_inner_size) {
      Derived::DoSplitFirst(block, old_inner_size);
    }
    block->SetFree(false);
    return BlockResult(Status::ResourceExhausted());
  }
  if (new_inner_size + Derived::kMinOuterSize <= block->InnerSize()) {
    // There is enough room after the resized block for another trailing block.
    Derived::DoSplitFirst(block, new_inner_size);
    result = result == BlockResult::Next::kMerged
                 ? BlockResult::Next::kResized
                 : BlockResult::Next::kSplitNew;
  }
  block->SetFree(false);
  return BlockResult(result);
}

template <typename Derived>
void AllocatableBlock<Derived>::Free(Derived*& block) {
  if (block == nullptr) {
    return;
  }
  block->CheckInvariants(/* crash_on_failure: */ true);
  Derived::DoFree(block);
  block->CheckInvariantsIfStrict();
}

template <typename Derived>
void AllocatableBlock<Derived>::DoFree(Derived*& block) {
  block->SetFree(true);

  // Try to merge the previous block with this one.
  Derived* prev = block->Prev();
  if (prev != nullptr && prev->IsFree()) {
    Derived::DoMergeNext(prev);
    block = prev;
  }

  // Try to merge this block with the next one.
  Derived* next = block->Next();
  if (next != nullptr && next->IsFree()) {
    Derived::DoMergeNext(block);
  }
}

}  // namespace pw::allocator
