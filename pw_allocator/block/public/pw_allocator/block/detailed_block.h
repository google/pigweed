// Copyright 2020 The Pigweed Authors
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
#include <limits>
#include <new>

#include "pw_allocator/block/alignable.h"
#include "pw_allocator/block/allocatable.h"
#include "pw_allocator/block/basic.h"
#include "pw_allocator/block/contiguous.h"
#include "pw_allocator/block/iterable.h"
#include "pw_allocator/block/poisonable.h"
#include "pw_allocator/block/result.h"
#include "pw_allocator/block/with_layout.h"
#include "pw_allocator/layout.h"
#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_preprocessor/compiler.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// Parameters type that encapsulates the block parameters.
///
/// @tparam   OffsetType  Unsigned integral type used to encode offsets. Larger
///                       types can address more memory, but consume greater
///                       overhead.
/// @tparam   WhenFree    Optional intrusive type that uses the block's usable
///                       space to track the block when free. This will affect
///                       the minimum alignment, and what portion of the usable
///                       space is skipped when poisoning.
template <typename OffsetType_, typename WhenFree>
struct DetailedBlockParameters {
  using OffsetType = std::make_unsigned_t<OffsetType_>;
  static constexpr Layout kLayoutWhenFree = Layout::Of<WhenFree>();
};

template <typename OffsetType_>
struct DetailedBlockParameters<OffsetType_, void> {
  using OffsetType = std::make_unsigned_t<OffsetType_>;
  static constexpr Layout kLayoutWhenFree = Layout(0, 1);
};

/// A block implementation that implements most optional block mix-ins.
///
/// This block dervies from various block mix-ins, allowing it to perform
/// aligned allocations, iterate over blocks, poison and check free blocks,
/// retrieve requested memory layouts, and more.
///
/// The amount of memory that can be addressed by a block of this type depends
/// on it given `OffsetType`. This type is used to describe the size of both the
/// current and previous block, so the maximum addressable range is given by
/// `std::numeric_limits<OffsetType> * alignof(OffsetType)`.
///
/// An additional 4 bytes are used to store details about an allocation,
/// including whether it is in use or free, whether it is poisoned, and what the
/// originally requested layout for a block was.
///
/// See also the `DetailedBlock` alias which provides the `Parameters` type
/// automatically.
///
/// @tparam   Parameters  Block traits, such as `DetailedBlockParameters`, which
///                   encapsulate a set of template parameters.
template <typename Parameters>
class DetailedBlockImpl
    : public BasicBlock<DetailedBlockImpl<Parameters>>,
      public ForwardIterableBlock<DetailedBlockImpl<Parameters>>,
      public ReverseIterableBlock<DetailedBlockImpl<Parameters>>,
      public ContiguousBlock<DetailedBlockImpl<Parameters>>,
      public AllocatableBlock<DetailedBlockImpl<Parameters>>,
      public AlignableBlock<DetailedBlockImpl<Parameters>>,
      public BlockWithLayout<DetailedBlockImpl<Parameters>>,
      public PoisonableBlock<DetailedBlockImpl<Parameters>> {
 private:
  using BlockType = DetailedBlockImpl<Parameters>;
  using Basic = BasicBlock<BlockType>;

 public:
  using OffsetType = typename Parameters::OffsetType;

  using Range = typename ForwardIterableBlock<BlockType>::Range;
  using Iterator = typename ForwardIterableBlock<BlockType>::Iterator;

  using ReverseRange = typename ReverseIterableBlock<BlockType>::ReverseRange;
  using ReverseIterator =
      typename ReverseIterableBlock<BlockType>::ReverseIterator;

 private:
  constexpr DetailedBlockImpl() : info_{} {
    info_.last = 1;
    info_.alignment = Basic::kAlignment;
  }

  // `Basic` required methods.
  friend Basic;
  static constexpr size_t DefaultAlignment() {
    return std::max(alignof(OffsetType),
                    Parameters::kLayoutWhenFree.alignment());
  }
  static constexpr size_t BlockOverhead() { return sizeof(BlockType); }
  static constexpr size_t MinInnerSize() { return 1; }
  static constexpr size_t ReservedWhenFree() {
    return Parameters::kLayoutWhenFree.size();
  }
  size_t OuterSizeUnchecked() const;
  size_t PrevOuterSizeUnchecked() const;

  // `Basic` overrides.
  bool DoCheckInvariants(bool crash_on_failure) const;

  // `Contiguous` required methods.
  using Contiguous = ContiguousBlock<BlockType>;
  friend Contiguous;

  static constexpr size_t MaxAddressableSize() {
    auto kOffsetMax =
        static_cast<size_t>(std::numeric_limits<OffsetType>::max());
    auto kSizeMax = static_cast<size_t>(std::numeric_limits<size_t>::max());
    return std::min(kSizeMax / Basic::kAlignment, kOffsetMax) *
           Basic::kAlignment;
  }

  constexpr bool IsLastUnchecked() const { return info_.last != 0; }
  static inline BlockType* AsBlock(ByteSpan bytes,
                                   BlockType* prev,
                                   BlockType* next);

  // `Contiguous` overrides.
  static inline BlockType* DoSplitFirst(BlockType*& block,
                                        size_t new_inner_size);
  static inline BlockType* DoSplitLast(BlockType*& block,
                                       size_t new_inner_size);
  static inline void DoMergeNext(BlockType*& block);

  // `Allocatable` required methods.
  using Allocatable = AllocatableBlock<BlockType>;
  friend Allocatable;
  constexpr bool IsFreeUnchecked() const { return info_.used == 0; }
  void SetFree(bool is_free);

  // `Alignable` overrides.
  using Alignable = AlignableBlock<BlockType>;
  friend Alignable;
  static inline StatusWithSize DoCanAlloc(const BlockType* block,
                                          Layout layout);
  static inline BlockResult DoAllocFirst(BlockType*& block, Layout layout);
  static inline BlockResult DoAllocLast(BlockType*& block, Layout layout);
  static inline BlockResult DoResize(BlockType*& block,
                                     size_t new_inner_size,
                                     bool shifted);
  static inline void DoFree(BlockType*& block);

  // `WithLayout` required methods.
  using WithLayout = BlockWithLayout<BlockType>;
  friend WithLayout;
  constexpr size_t RequestedSize() const {
    return Basic::InnerSize() - padding_;
  }
  constexpr size_t RequestedAlignment() const { return info_.alignment; }
  void SetRequestedSize(size_t size);
  void SetRequestedAlignment(size_t alignment);

  // `Poisonable` required methods.
  using Poisonable = PoisonableBlock<BlockType>;
  friend Poisonable;
  constexpr bool IsPoisonedUnchecked() const { return info_.poisoned != 0; }
  constexpr void SetPoisoned(bool is_poisoned) { info_.poisoned = is_poisoned; }

  /// Offset (in increments of the minimum alignment) from this block to the
  /// previous block. 0 if this is the first block.
  OffsetType prev_ = 0;

  /// Offset (in increments of the minimum alignment) from this block to the
  /// next block. Valid even if this is the last block, since it equals the
  /// size of the block.
  OffsetType next_ = 0;

  /// Information about the current state of the block:
  /// * If the `used` flag is set, the block's usable memory has been allocated
  ///   and is being used.
  /// * If the `poisoned` flag is set and the `used` flag is clear, the block's
  ///   usable memory contains a poison pattern that will be checked when the
  ///   block is allocated.
  /// * If the `last` flag is set, the block does not have a next block.
  /// * If the `used` flag is set, the alignment represents the requested value
  ///   when the memory was allocated, which may be less strict than the actual
  ///   alignment.
  struct {
    uint16_t used : 1;
    uint16_t poisoned : 1;
    uint16_t last : 1;
    uint16_t alignment : 13;
  } info_;

  /// Number of bytes allocated beyond what was requested. This will be at most
  /// the minimum alignment, i.e. `alignof(OffsetType).`
  uint16_t padding_ = 0;
};

// Convenience alias that constructs the block traits automatically.
template <typename OffsetType = uintptr_t, typename WhenFree = void>
using DetailedBlock =
    DetailedBlockImpl<DetailedBlockParameters<OffsetType, WhenFree>>;

// Template method implementations.

// `Basic` methods.

template <typename Parameters>
size_t DetailedBlockImpl<Parameters>::OuterSizeUnchecked() const {
  size_t outer_size;
  PW_ASSERT(!PW_MUL_OVERFLOW(next_, Basic::kAlignment, &outer_size));
  return outer_size;
}

template <typename Parameters>
size_t DetailedBlockImpl<Parameters>::PrevOuterSizeUnchecked() const {
  size_t outer_size;
  PW_ASSERT(!PW_MUL_OVERFLOW(prev_, Basic::kAlignment, &outer_size));
  return outer_size;
}

template <typename Parameters>
bool DetailedBlockImpl<Parameters>::DoCheckInvariants(
    bool crash_on_failure) const {
  return Basic::DoCheckInvariants(crash_on_failure) &&
         Contiguous::DoCheckInvariants(crash_on_failure) &&
         Poisonable::DoCheckInvariants(crash_on_failure);
}

// `Contiguous` methods.

template <typename Parameters>
DetailedBlockImpl<Parameters>* DetailedBlockImpl<Parameters>::AsBlock(
    ByteSpan bytes, BlockType* prev, BlockType* next) {
  size_t prev_offset = 0;
  if (prev != nullptr) {
    prev_offset = prev->OuterSize();
    prev->info_.last = 0;
  }
  size_t next_offset = bytes.size();
  auto* block = ::new (bytes.data()) DetailedBlockImpl();
  block->prev_ = prev_offset / Basic::kAlignment;
  block->next_ = next_offset / Basic::kAlignment;
  if (next != nullptr) {
    block->info_.last = 0;
    next->prev_ = block->next_;
  }
  return block;
}

template <typename Parameters>
DetailedBlockImpl<Parameters>* DetailedBlockImpl<Parameters>::DoSplitFirst(
    DetailedBlockImpl*& block, size_t new_inner_size) {
  return Poisonable::DoSplitFirst(block, new_inner_size);
}

template <typename Parameters>
DetailedBlockImpl<Parameters>* DetailedBlockImpl<Parameters>::DoSplitLast(
    DetailedBlockImpl*& block, size_t new_inner_size) {
  return Poisonable::DoSplitLast(block, new_inner_size);
}

template <typename Parameters>
void DetailedBlockImpl<Parameters>::DoMergeNext(DetailedBlockImpl*& block) {
  Poisonable::DoMergeNext(block);
}

// `Allocatable` methods.

template <typename Parameters>
void DetailedBlockImpl<Parameters>::SetFree(bool is_free) {
  info_.used = !is_free;
  Poisonable::SetFree(is_free);
}

// `Alignable` methods.

template <typename Parameters>
StatusWithSize DetailedBlockImpl<Parameters>::DoCanAlloc(
    const DetailedBlockImpl* block, Layout layout) {
  return Alignable::DoCanAlloc(block, layout);
}

template <typename Parameters>
BlockResult DetailedBlockImpl<Parameters>::DoAllocFirst(
    DetailedBlockImpl*& block, Layout layout) {
  return WithLayout::DoAllocFirst(block, layout);
}

template <typename Parameters>
BlockResult DetailedBlockImpl<Parameters>::DoAllocLast(
    DetailedBlockImpl*& block, Layout layout) {
  return WithLayout::DoAllocLast(block, layout);
}

template <typename Parameters>
BlockResult DetailedBlockImpl<Parameters>::DoResize(DetailedBlockImpl*& block,
                                                    size_t new_inner_size,
                                                    bool shifted) {
  return WithLayout::DoResize(block, new_inner_size, shifted);
}

template <typename Parameters>
void DetailedBlockImpl<Parameters>::DoFree(DetailedBlockImpl*& block) {
  return WithLayout::DoFree(block);
}

// `WithLayout` methods.

template <typename Parameters>
void DetailedBlockImpl<Parameters>::SetRequestedSize(size_t size) {
  size_t inner_size = Basic::InnerSize();
  size_t padding;
  PW_ASSERT(!PW_SUB_OVERFLOW(inner_size, size, &padding));
  PW_ASSERT(padding <= std::numeric_limits<uint16_t>::max());
  padding_ = static_cast<uint16_t>(padding);
}

template <typename Parameters>
void DetailedBlockImpl<Parameters>::SetRequestedAlignment(size_t alignment) {
  PW_ASSERT((alignment & (alignment - 1)) == 0);
  PW_ASSERT(alignment < 0x2000);
  info_.alignment = alignment;
}

}  // namespace pw::allocator
