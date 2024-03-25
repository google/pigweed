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

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_allocator/capability.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::allocator {
namespace internal {

/// Block-independent base class of ``BlockAllocator``.
///
/// This class contains static methods which do not depend on the template
/// parameters of ``BlockAllocator`` that are used to determine block/ type.
/// This allows the methods to be defined in a separate source file and use
/// macros that cannot be used in headers, e.g. PW_CHECK.
///
/// This class should not be used directly. Instead, use ``BlockAllocator`` or
/// one of its specializations.
class GenericBlockAllocator : public Allocator {
 public:
  static constexpr Capabilities kCapabilities =
      Capability::kImplementsGetLayout | Capability::kImplementsQuery;

  // Not copyable or movable.
  GenericBlockAllocator(const GenericBlockAllocator&) = delete;
  GenericBlockAllocator& operator=(const GenericBlockAllocator&) = delete;
  GenericBlockAllocator(GenericBlockAllocator&&) = delete;
  GenericBlockAllocator& operator=(GenericBlockAllocator&&) = delete;

 protected:
  constexpr GenericBlockAllocator() : Allocator(kCapabilities) {}

  /// Crashes with an informational method that the given block is allocated.
  ///
  /// This method is meant to be called by ``SplitFreeListAllocator``s
  /// destructor. There must not be any outstanding allocations from an
  /// when it is destroyed.
  static void CrashOnAllocated(void* allocated);
};

/// A memory allocator that uses a list of blocks.
///
/// This class does not implement `ChooseBlock` and cannot be used directly.
/// Instead, use one of its specializations.
///
/// NOTE!! Do NOT use memory returned from this allocator as the backing for
/// another allocator. If this is done, the `Query` method may incorrectly
/// think pointers returned by that allocator were created by this one, and
/// report that this allocator can de/reallocate them.
template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
class BlockAllocator : public GenericBlockAllocator {
 public:
  using BlockType = Block<OffsetType, kAlign, kPoisonInterval != 0>;
  using Range = typename BlockType::Range;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr BlockAllocator() : GenericBlockAllocator() {}

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// Errors are fatal.
  ///
  /// @param[in]  region              The memory region for this allocator.
  explicit BlockAllocator(ByteSpan region) : BlockAllocator() {
    PW_ASSERT(Init(region).ok());
  }

  ~BlockAllocator() override { Reset(); }

  /// Returns a ``Range`` of blocks tracking the memory of this allocator.
  Range blocks() const;

  /// Sets the memory region to be used by this allocator.
  ///
  /// This method will instantiate an initial block using the memory region.
  ///
  /// @param[in]  region              The memory region for this allocator.
  /// @retval     OK                  The allocator is initialized.
  /// @retval     INVALID_ARGUMENT    The memory region is null.
  /// @retval     RESOURCE_EXHAUSTED  The region is too small for `BlockType`.
  /// @retval     OUT_OF_RANGE        The region too large for `BlockType`.
  Status Init(ByteSpan region);

  /// Sets the blocks to be used by this allocator.
  ///
  /// This method will use the sequence blocks as-is, which must be valid. If
  /// `end` is not provided, the sequence extends to a block marked "last".
  ///
  /// @param[in]  region              The memory region for this allocator.
  /// @retval     OK                  The allocator is initialized.
  /// @retval     INVALID_ARGUMENT    The block sequence is empty.
  Status Init(BlockType* begin, BlockType* end = nullptr);

  /// Initializes the allocator with preconfigured blocks.
  ///
  /// This method does not

  /// Resets the allocator to an uninitialized state.
  ///
  /// At the time of the call, there MUST NOT be any outstanding allocated
  /// blocks from this allocator.
  void Reset();

 protected:
  using ReverseRange = typename BlockType::ReverseRange;

  /// Returns a ``ReverseRange`` of blocks tracking the memory of this
  /// allocator.
  ReverseRange rblocks();

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  /// @copydoc Allocator::GetLayout
  Result<Layout> DoGetLayout(const void* ptr) const override;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  /// Selects a free block to allocate from.
  ///
  /// This method represents the allocator-specific strategy of choosing which
  /// block should be used to satisfy allocation requests.
  ///
  /// @param  layout  Same as ``Allocator::Allocate``.
  virtual BlockType* ChooseBlock(Layout layout) = 0;

  /// Returns the block associated with a pointer.
  ///
  /// If the given pointer is to this allocator's memory region, but not to a
  /// valid block, the memory is corrupted and this method will crash to assist
  /// in uncovering the underlying bug.
  ///
  /// @param  ptr           Pointer to an allocated block's usable space.
  /// @retval OK            Result contains a pointer to the block.
  /// @retval OUT_OF_RANGE  Given pointer is outside the allocator's memory.
  template <typename PtrType,
            typename BlockPtrType = std::conditional_t<
                std::is_const_v<std::remove_pointer_t<PtrType>>,
                const BlockType*,
                BlockType*>>
  Result<BlockPtrType> FromUsableSpace(PtrType ptr) const;

  /// Ensures the pointer to the last block is correct after the given block is
  /// allocated or freed.
  void UpdateLast(BlockType* block);

  // Represents the range of blocks for this allocator.
  BlockType* first_ = nullptr;
  BlockType* last_ = nullptr;
  uint16_t unpoisoned_ = 0;
};

}  // namespace internal

/// Block allocator that uses a "first-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by starting at
/// the beginning of the range of blocks and looking for the first one which can
/// satisfy the request.
///
/// This strategy may result in slightly worse fragmentation than the
/// corresponding "last-fit" strategy, since the alignment may result in unused
/// fragments both before and after an allocated block.
template <typename OffsetType = uintptr_t,
          size_t kPoisonInterval = 0,
          size_t kAlign = alignof(OffsetType)>
class FirstFitBlockAllocator
    : public internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
  using BlockType = typename Base::BlockType;

  constexpr FirstFitBlockAllocator() : Base() {}
  explicit FirstFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    // Search forwards for the first block that can hold this allocation.
    for (auto* block : Base::blocks()) {
      if (BlockType::AllocFirst(block, layout.size(), layout.alignment())
              .ok()) {
        return block;
      }
    }
    return nullptr;
  }
};

/// Block allocator that uses a "last-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by starting at
/// the end of the range of blocks and looking for the last one which can
/// satisfy the request.
///
/// This strategy may result in slightly better fragmentation than the
/// corresponding "first-fit" strategy, since even with alignment it will result
/// in at most one unused fragment before the allocated block.
template <typename OffsetType = uintptr_t,
          size_t kPoisonInterval = 0,
          size_t kAlign = alignof(OffsetType)>
class LastFitBlockAllocator
    : public internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
  using BlockType = typename Base::BlockType;

  constexpr LastFitBlockAllocator() : Base() {}
  explicit LastFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    // Search backwards for the last block that can hold this allocation.
    for (auto* block : Base::rblocks()) {
      if (BlockType::AllocLast(block, layout.size(), layout.alignment()).ok()) {
        return block;
      }
    }
    return nullptr;
  }
};

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
          size_t kPoisonInterval = 0,
          size_t kAlign = alignof(OffsetType)>
class BestFitBlockAllocator
    : public internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
  using BlockType = typename Base::BlockType;

  constexpr BestFitBlockAllocator() : Base() {}
  explicit BestFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    // Search backwards for the smallest block that can hold this allocation.
    BlockType* best = nullptr;
    for (auto* block : Base::rblocks()) {
      if (!block->CanAllocLast(layout.size(), layout.alignment()).ok()) {
        continue;
      }
      if (best == nullptr || block->OuterSize() < best->OuterSize()) {
        best = block;
      }
    }
    if (best != nullptr &&
        BlockType::AllocLast(best, layout.size(), layout.alignment()).ok()) {
      return best;
    }
    return nullptr;
  }
};

/// Block allocator that uses a "worst-fit" allocation strategy.
///
/// In this strategy, the allocator handles an allocation request by looking at
/// all unused blocks and finding the biggest one which can satisfy the
/// request.
///
/// This algorithm may lead to less fragmentation as any unused fragments are
/// more likely to be large enough to be useful to other requests.
template <typename OffsetType = uintptr_t,
          size_t kPoisonInterval = 0,
          size_t kAlign = alignof(OffsetType)>
class WorstFitBlockAllocator
    : public internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
  using BlockType = typename Base::BlockType;

  constexpr WorstFitBlockAllocator() : Base() {}
  explicit WorstFitBlockAllocator(ByteSpan region) : Base(region) {}

 private:
  /// @copydoc Allocator::Allocate
  BlockType* ChooseBlock(Layout layout) override {
    // Search backwards for the biggest block that can hold this allocation.
    BlockType* worst = nullptr;
    for (auto* block : Base::rblocks()) {
      if (!block->CanAllocLast(layout.size(), layout.alignment()).ok()) {
        continue;
      }
      if (worst == nullptr || block->OuterSize() > worst->OuterSize()) {
        worst = block;
      }
    }
    if (worst != nullptr &&
        BlockType::AllocLast(worst, layout.size(), layout.alignment()).ok()) {
      return worst;
    }
    return nullptr;
  }
};

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
    : public internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign> {
 public:
  using Base = internal::BlockAllocator<OffsetType, kPoisonInterval, kAlign>;
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

// Template method implementations

namespace internal {

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
typename BlockAllocator<OffsetType, kPoisonInterval, kAlign>::Range
BlockAllocator<OffsetType, kPoisonInterval, kAlign>::blocks() const {
  return Range(first_);
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
typename BlockAllocator<OffsetType, kPoisonInterval, kAlign>::ReverseRange
BlockAllocator<OffsetType, kPoisonInterval, kAlign>::rblocks() {
  while (last_ != nullptr && !last_->Last()) {
    last_ = last_->Next();
  }
  return ReverseRange(last_);
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
Status BlockAllocator<OffsetType, kPoisonInterval, kAlign>::Init(
    ByteSpan region) {
  Result<BlockType*> result = BlockType::Init(region);
  if (!result.ok()) {
    return result.status();
  }
  return Init(*result);
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
Status BlockAllocator<OffsetType, kPoisonInterval, kAlign>::Init(
    BlockType* begin, BlockType* end) {
  if (begin == nullptr) {
    return Status::InvalidArgument();
  }
  if (end == nullptr) {
    end = begin;
    while (!end->Last()) {
      end = end->Next();
    }
  } else if (begin < end) {
    end->MarkLast();
  } else {
    return Status::InvalidArgument();
  }
  first_ = begin;
  last_ = end;
  return OkStatus();
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
void BlockAllocator<OffsetType, kPoisonInterval, kAlign>::Reset() {
  for (auto* block : blocks()) {
    if (block->Used()) {
      CrashOnAllocated(block);
    }
  }
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
void* BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoAllocate(
    Layout layout) {
  BlockType* block = ChooseBlock(layout);
  if (block == nullptr) {
    return nullptr;
  }
  UpdateLast(block);
  return block->UsableSpace();
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
void BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoDeallocate(
    void* ptr, Layout) {
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    return;
  }
  BlockType* block = *result;

  // Free the block and merge it with its neighbors, if possible.
  BlockType::Free(block);
  UpdateLast(block);

  if constexpr (kPoisonInterval != 0) {
    ++unpoisoned_;
    if (unpoisoned_ >= kPoisonInterval) {
      block->Poison();
      unpoisoned_ = 0;
    }
  }
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
bool BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoResize(
    void* ptr, Layout, size_t new_size) {
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    return false;
  }
  BlockType* block = *result;

  if (!BlockType::Resize(block, new_size).ok()) {
    return false;
  }
  UpdateLast(block);
  return true;
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
Result<Layout> BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoGetLayout(
    const void* ptr) const {
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    return result.status();
  }
  return (*result)->GetLayout();
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
Status BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoQuery(
    const void* ptr, Layout) const {
  return FromUsableSpace(ptr).status();
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
template <typename PtrType, typename BlockPtrType>
Result<BlockPtrType>
BlockAllocator<OffsetType, kPoisonInterval, kAlign>::FromUsableSpace(
    PtrType ptr) const {
  if (ptr < first_->UsableSpace() || last_->UsableSpace() < ptr) {
    return Status::OutOfRange();
  }
  BlockPtrType block = BlockType::FromUsableSpace(ptr);
  block->CrashIfInvalid();
  return block;
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
void BlockAllocator<OffsetType, kPoisonInterval, kAlign>::UpdateLast(
    BlockType* block) {
  if (block->Last()) {
    last_ = block;
  } else if (block->Next()->Last()) {
    last_ = block->Next();
  }
}

}  // namespace internal
}  // namespace pw::allocator
