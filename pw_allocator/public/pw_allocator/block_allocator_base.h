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
  static constexpr Capabilities kCapabilities = kImplementsGetUsableLayout |
                                                kImplementsGetAllocatedLayout |
                                                kImplementsQuery;

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
template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
class BlockAllocator : public internal::GenericBlockAllocator {
 public:
  using BlockType = Block<OffsetType, kAlign, kPoisonInterval != 0>;
  using Range = typename BlockType::Range;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr BlockAllocator() : internal::GenericBlockAllocator() {}

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
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The allocator is initialized.
  ///
  ///    INVALID_ARGUMENT: The memory region is null.
  ///
  ///    RESOURCE_EXHAUSTED: The region is too small for ``BlockType``.
  ///
  ///    OUT_OF_RANGE: The region too large for ``BlockType``.
  ///
  /// @endrst
  Status Init(ByteSpan region);

  /// Sets the blocks to be used by this allocator.
  ///
  /// This method will use the sequence blocks as-is, which must be valid. If
  /// `end` is not provided, the sequence extends to a block marked "last".
  ///
  /// @param[in]  region              The memory region for this allocator.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The allocator is initialized.
  ///
  ///    INVALID_ARGUMENT: The block sequence is empty.
  ///
  /// @endrst
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
  void DoDeallocate(void* ptr) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override { DoDeallocate(ptr); }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override;

  /// @copydoc Allocator::GetUsableLayout
  StatusWithSize DoGetCapacity() const override {
    return StatusWithSize(capacity_);
  }

  /// @copydoc Allocator::GetUsableLayout
  Result<Layout> DoGetUsableLayout(const void* ptr) const override;

  /// @copydoc Allocator::GetAllocatedLayout
  Result<Layout> DoGetAllocatedLayout(const void* ptr) const override;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr) const override;

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
  for (const auto& block : blocks()) {
    capacity_ += block->OuterSize();
  }
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
    void* ptr) {
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
    void* ptr, size_t new_size) {
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
Result<Layout>
BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoGetUsableLayout(
    const void* ptr) const {
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    return Status::NotFound();
  }
  const BlockType* block = result.value();
  if (!block->Used()) {
    return Status::FailedPrecondition();
  }
  return Layout(block->InnerSize(), block->Alignment());
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
Result<Layout>
BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoGetAllocatedLayout(
    const void* ptr) const {
  auto result = FromUsableSpace(ptr);
  if (!result.ok()) {
    return Status::NotFound();
  }
  const BlockType* block = result.value();
  if (!block->Used()) {
    return Status::FailedPrecondition();
  }
  return Layout(block->OuterSize(), block->Alignment());
}

template <typename OffsetType, uint16_t kPoisonInterval, uint16_t kAlign>
Status BlockAllocator<OffsetType, kPoisonInterval, kAlign>::DoQuery(
    const void* ptr) const {
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

}  // namespace pw::allocator
