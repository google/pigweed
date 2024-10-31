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

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_allocator/capability.h"
#include "pw_allocator/fragmentation.h"
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
      kImplementsGetRequestedLayout | kImplementsGetUsableLayout |
      kImplementsGetAllocatedLayout | kImplementsGetCapacity |
      kImplementsRecognizes;

  // Not copyable or movable.
  GenericBlockAllocator(const GenericBlockAllocator&) = delete;
  GenericBlockAllocator& operator=(const GenericBlockAllocator&) = delete;
  GenericBlockAllocator(GenericBlockAllocator&&) = delete;
  GenericBlockAllocator& operator=(GenericBlockAllocator&&) = delete;

 protected:
  constexpr GenericBlockAllocator() : Allocator(kCapabilities) {}

  /// Crashes with an informational message that a given block is allocated.
  ///
  /// This method is meant to be called by ``SplitFreeListAllocator``s
  /// destructor. There must not be any outstanding allocations from an
  /// when it is destroyed.
  static void CrashOnAllocated(void* allocated);

  /// Crashes with an informational message that a given pointer does not belong
  /// to this allocator.
  static void CrashOnInvalidFree(void* freed);

  /// Crashes with an informational message that a given block was freed twice.
  static void CrashOnDoubleFree(void* freed);
};

}  // namespace internal

/// A memory allocator that uses a list of blocks.
///
/// This class does not implement `ChooseBlock` and cannot be used directly.
/// Instead, use one of its specializations.
///
/// NOTE!! Do NOT use memory returned from this allocator as the backing for
/// another allocator. If this is done, the `Query` method may incorrectly
/// think pointers returned by that allocator were created by this one, and
/// report that this allocator can de/reallocate them.
template <typename OffsetType, uint16_t kPoisonInterval>
class BlockAllocator : public internal::GenericBlockAllocator {
 public:
  using BlockType = Block<OffsetType>;
  using Range = typename BlockType::Range;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr BlockAllocator() : internal::GenericBlockAllocator() {}

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be large enough to fit an
  ///                     aligned block with overhead. It MUST NOT be larger
  ///                     than what is addressable by `OffsetType`.
  explicit BlockAllocator(ByteSpan region) : BlockAllocator() { Init(region); }

  ~BlockAllocator() override { Reset(); }

  /// Returns a ``Range`` of blocks tracking the memory of this allocator.
  Range blocks() const;

  /// Sets the memory region to be used by this allocator.
  ///
  /// This method will instantiate an initial block using the memory region.
  ///
  /// @param[in]  region  Region of memory to use when satisfying allocation
  ///                     requests. The region MUST be large enough to fit an
  ///                     aligned block with overhead. It MUST NOT be larger
  ///                     than what is addressable by `OffsetType`.
  void Init(ByteSpan region);

  /// Sets the blocks to be used by this allocator.
  ///
  /// This method will use the sequence of blocks including and following
  /// `begin`. These blocks which must be valid.
  ///
  /// @param[in]  begin               The first block for this allocator.
  ///                                 The block must not have a previous block.
  void Init(BlockType* begin) { return Init(begin, nullptr); }

  /// Sets the blocks to be used by this allocator.
  ///
  /// This method will use the sequence blocks as-is, which must be valid.
  ///
  /// @param[in]  begin   The first block for this allocator.
  /// @param[in]  end     The last block for this allocator. May be null, in
  ///                     which the sequence including and following `begin` is
  ///                     used. If not null, the block must not have a next
  ///                     block.
  virtual void Init(BlockType* begin, BlockType* end);

  /// Returns fragmentation information for the block allocator's memory region.
  Fragmentation MeasureFragmentation() const;

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

  /// Returns the block associated with a pointer.
  ///
  /// If the given pointer is to this allocator's memory region, but not to a
  /// valid block, the memory is corrupted and this method will crash to assist
  /// in uncovering the underlying bug.
  ///
  /// @param  ptr           Pointer to an allocated block's usable space.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Result contains a pointer to the block.
  ///
  ///    OUT_OF_RANGE: Given pointer is outside the allocator's memory.
  ///
  /// @endrst
  template <typename PtrType,
            typename BlockPtrType = std::conditional_t<
                std::is_const_v<std::remove_pointer_t<PtrType>>,
                const BlockType*,
                BlockType*>>
  Result<BlockPtrType> FromUsableSpace(PtrType ptr) const;

 private:
  /// Indicates that a block will no longer be free.
  ///
  /// Does nothing by default. Derived class may overload to do additional
  /// bookkeeeping.
  ///
  /// @param  block   The block being freed.
  virtual void ReserveBlock(BlockType*) {}

  /// Indicates that a block is now free.
  ///
  /// Does nothing by default. Derived class may overload to do additional
  /// bookkeeeping.
  ///
  /// @param  block   The block being freed.
  virtual void RecycleBlock(BlockType*) {}

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override { DoDeallocate(ptr); }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override;

  /// @copydoc Deallocator::GetInfo
  Result<Layout> DoGetInfo(InfoType info_type, const void* ptr) const override;

  /// Selects a free block to allocate from.
  ///
  /// This method represents the allocator-specific strategy of choosing which
  /// block should be used to satisfy allocation requests.
  ///
  /// If derived classes override ``ReserveBlock`` and ``RecycleBlock`` to
  /// provide additional bookkeeping, the implementation of this method should
  /// invoke those methods as needed.
  ///
  /// @param  layout  Same as ``Allocator::Allocate``.
  virtual BlockType* ChooseBlock(Layout layout) = 0;

  /// Returns if the previous block exists and is free.
  static bool PrevIsFree(const BlockType* block) {
    auto* prev = block->Prev();
    return prev != nullptr && prev->IsFree();
  }

  /// Returns if the next block exists and is free.
  static bool NextIsFree(const BlockType* block) {
    auto* next = block->Next();
    return next != nullptr && next->IsFree();
  }

  /// Ensures the pointer to the last block is correct after the given block is
  /// allocated or freed.
  void UpdateLast(BlockType* block);

  // Represents the range of blocks for this allocator.
  size_t capacity_ = 0;
  BlockType* first_ = nullptr;
  BlockType* last_ = nullptr;
  uint16_t unpoisoned_ = 0;
};

// Template method implementations

template <typename OffsetType, uint16_t kPoisonInterval>
typename BlockAllocator<OffsetType, kPoisonInterval>::Range
BlockAllocator<OffsetType, kPoisonInterval>::blocks() const {
  return Range(first_);
}

template <typename OffsetType, uint16_t kPoisonInterval>
typename BlockAllocator<OffsetType, kPoisonInterval>::ReverseRange
BlockAllocator<OffsetType, kPoisonInterval>::rblocks() {
  PW_ASSERT(last_ == nullptr || last_->Next() == nullptr);
  return ReverseRange(last_);
}

template <typename OffsetType, uint16_t kPoisonInterval>
void BlockAllocator<OffsetType, kPoisonInterval>::Init(ByteSpan region) {
  Result<BlockType*> result = BlockType::Init(region);
  Init(*result, nullptr);
}

template <typename OffsetType, uint16_t kPoisonInterval>
void BlockAllocator<OffsetType, kPoisonInterval>::Init(BlockType* begin,
                                                       BlockType* end) {
  PW_ASSERT(begin != nullptr);
  PW_ASSERT(begin->Prev() == nullptr);
  if (end == nullptr) {
    end = begin;
    for (BlockType* next = end->Next(); next != nullptr; next = end->Next()) {
      end = next;
    }
  } else {
    PW_ASSERT(begin <= end);
    PW_ASSERT(end->Next() == nullptr);
  }
  first_ = begin;
  last_ = end;
  for (const auto& block : blocks()) {
    capacity_ += block->OuterSize();
  }
}

template <typename OffsetType, uint16_t kPoisonInterval>
void BlockAllocator<OffsetType, kPoisonInterval>::Reset() {
  for (auto* block : blocks()) {
    if (!block->IsFree()) {
      CrashOnAllocated(block);
    }
  }
}

template <typename OffsetType, uint16_t kPoisonInterval>
void* BlockAllocator<OffsetType, kPoisonInterval>::DoAllocate(Layout layout) {
  PW_CHECK_PTR_EQ(last_->Next(), nullptr);
  BlockType* block = ChooseBlock(layout);
  if (block == nullptr) {
    return nullptr;
  }
  UpdateLast(block);
  PW_CHECK_PTR_LE(block, last_);
  return block->UsableSpace();
}

template <typename OffsetType, uint16_t kPoisonInterval>
void BlockAllocator<OffsetType, kPoisonInterval>::DoDeallocate(void* ptr) {
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    CrashOnInvalidFree(ptr);
  }
  BlockType* block = *result;
  if (block->IsFree()) {
    CrashOnDoubleFree(block);
  }

  // Neighboring blocks may be merged when freeing.
  if (PrevIsFree(block)) {
    ReserveBlock(block->Prev());
  }
  if (NextIsFree(block)) {
    ReserveBlock(block->Next());
  }

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

  RecycleBlock(block);
}

template <typename OffsetType, uint16_t kPoisonInterval>
bool BlockAllocator<OffsetType, kPoisonInterval>::DoResize(void* ptr,
                                                           size_t new_size) {
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    return false;
  }
  BlockType* block = *result;

  // Neighboring blocks may be merged when resizing.
  if (NextIsFree(block)) {
    ReserveBlock(block->Next());
  }

  if (!BlockType::Resize(block, new_size).ok()) {
    return false;
  }
  UpdateLast(block);

  if (NextIsFree(block)) {
    RecycleBlock(block->Next());
  }

  return true;
}

template <typename OffsetType, uint16_t kPoisonInterval>
Result<Layout> BlockAllocator<OffsetType, kPoisonInterval>::DoGetInfo(
    InfoType info_type, const void* ptr) const {
  // Handle types not related to a block first.
  if (info_type == InfoType::kCapacity) {
    return Layout(capacity_);
  }
  // Get a block from the given pointer.
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    return Status::NotFound();
  }
  const BlockType* block = result.value();
  if (block->IsFree()) {
    return Status::FailedPrecondition();
  }
  switch (info_type) {
    case InfoType::kRequestedLayoutOf:
      return Layout(block->RequestedSize(), block->Alignment());
    case InfoType::kUsableLayoutOf:
      return Layout(block->InnerSize(), block->Alignment());
    case InfoType::kAllocatedLayoutOf:
      return Layout(block->OuterSize(), block->Alignment());
    case InfoType::kRecognizes:
      return Layout();
    case InfoType::kCapacity:
    default:
      return Status::Unimplemented();
  }
}

template <typename OffsetType, uint16_t kPoisonInterval>
Fragmentation
BlockAllocator<OffsetType, kPoisonInterval>::MeasureFragmentation() const {
  Fragmentation fragmentation;
  for (auto block : blocks()) {
    if (block->IsFree()) {
      fragmentation.AddFragment(block->InnerSize() / BlockType::kAlignment);
    }
  }
  return fragmentation;
}

template <typename OffsetType, uint16_t kPoisonInterval>
template <typename PtrType, typename BlockPtrType>
Result<BlockPtrType>
BlockAllocator<OffsetType, kPoisonInterval>::FromUsableSpace(
    PtrType ptr) const {
  if (ptr < first_->UsableSpace() || last_->UsableSpace() < ptr) {
    return Status::OutOfRange();
  }
  return BlockType::FromUsableSpace(ptr);
}

template <typename OffsetType, uint16_t kPoisonInterval>
void BlockAllocator<OffsetType, kPoisonInterval>::UpdateLast(BlockType* block) {
  BlockType* next = block->Next();
  if (next == nullptr) {
    last_ = block;
  } else if (next->Next() == nullptr) {
    last_ = next;
  }
}

}  // namespace pw::allocator
