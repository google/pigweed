// Copyright 2023 The Pigweed Authors
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// Block-independent base class of SplitFreeListAllocator.
///
/// This class contains static methods which do not depend on the template
/// parameters of ``SplitFreeListAllocator`` that are used to determine block
/// type. This allows the methods to be defined in a separate source file and
/// use macros that cannot be used in headers, e.g. PW_CHECK.
///
/// This class should not be used directly. Instead, see
/// ``SplitFreeListAllocator``.
class BaseSplitFreeListAllocator : public Allocator {
 protected:
  constexpr BaseSplitFreeListAllocator() = default;

  /// Crashes with an informational method that the given block is allocated.
  ///
  /// This method is meant to be called by ``SplitFreeListAllocator``s
  /// destructor. There must not be any outstanding allocations from an
  /// when it is destroyed.
  static void CrashOnAllocated(void* allocated);
};

/// This memory allocator uses a free list to track unallocated blocks, with a
/// twist: Allocations above or below a given threshold are taken from
/// respectively lower or higher addresses from within the allocator's memory
/// region. This has the effect of decreasing fragmentation as similarly-sized
/// allocations are grouped together.
///
/// NOTE!! Do NOT use memory returned from this allocator as the backing for
/// another allocator. If this is done, the `Query` method will incorrectly
/// think pointers returned by that alloator were created by this one, and
/// report that this allocator can de/reallocate them.
template <typename BlockType = Block<>>
class SplitFreeListAllocator : public BaseSplitFreeListAllocator {
 public:
  using Range = typename BlockType::Range;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr SplitFreeListAllocator() = default;

  /// Non-constexpr constructor that automatically calls `Init`.
  ///
  /// Errors are fatal.
  ///
  /// @param[in]  region              The memory region for this allocator.
  /// @param[in]  threshold           Allocations of this size of larger are
  ///                                 "large" and come from lower addresses.
  explicit SplitFreeListAllocator(ByteSpan region, size_t threshold)
      : SplitFreeListAllocator() {
    PW_ASSERT(Init(region, threshold).ok());
  }

  ~SplitFreeListAllocator() override;

  // Not copyable.
  SplitFreeListAllocator(const SplitFreeListAllocator&) = delete;
  SplitFreeListAllocator& operator=(const SplitFreeListAllocator&) = delete;

  /// Sets the memory region to be used by this allocator, and the threshold at
  /// which allocations are considerd "large" or "small". Large and small
  /// allocations return lower and higher addresses, respectively.
  ///
  /// @param[in]  region              The memory region for this allocator.
  /// @param[in]  threshold           Allocations of this size of larger are
  ///                                 "large" and come from lower addresses.
  /// @retval     OK                  The allocator is initialized.
  /// @retval     INVALID_ARGUMENT    The memory region is null.
  /// @retval     RESOURCE_EXHAUSTED  The region is too small for `BlockType`.
  /// @retval     OUT_OF_RANGE        The region too large for `BlockType`.
  Status Init(ByteSpan region, size_t threshold = 0);

  /// Returns an iterable range of blocks tracking the memory of this allocator.
  Range blocks() const;

 private:
  using ReverseRange = typename BlockType::ReverseRange;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// Allocate a large chunk of memory.
  ///
  /// Allocations larger than the threshold will be allocated from lower
  /// addresses. If a block needs to be fragmented, the returned pointer will be
  /// from the lower-addressed part of the block.
  ///
  /// @param[in]  layout      Describes the memory to be allocated.
  void* DoAllocateLarge(Layout layout);

  /// Allocate a small chunk of memory.
  ///
  /// Allocations smaller than the threshold will be allocated from higher
  /// addresses. If a block needs to be fragmented, the returned pointer will be
  /// from the higher-addressed part of the block.
  ///
  /// @param[in]  layout      Describes the memory to be allocated.
  void* DoAllocateSmall(Layout layout);

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  /// @copydoc Allocator::GetLayout
  Result<Layout> DoGetLayout(const void* ptr) const override;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  // Represents the entire memory region for this allocator.
  void* begin_ = nullptr;
  void* end_ = nullptr;

  // Represents the range of blocks that include free blocks. Blocks outside
  // this range are guaranteed to be in use. These are effectively cached values
  // used to speed up searching for free blocks.
  BlockType* first_free_ = nullptr;
  BlockType* last_free_ = nullptr;

  // The boundary between what are consider "small" and "large" allocations.
  size_t threshold_ = 0;
};

// Template method implementations

template <typename BlockType>
SplitFreeListAllocator<BlockType>::~SplitFreeListAllocator() {
  for (auto* block : blocks()) {
    if (block->Used()) {
      CrashOnAllocated(block);
    }
  }
}

template <typename BlockType>
typename BlockType::Range SplitFreeListAllocator<BlockType>::blocks() const {
  auto* begin =
      begin_ == nullptr
          ? nullptr
          : BlockType::FromUsableSpace(static_cast<std::byte*>(begin_));
  return Range(begin);
}

template <typename BlockType>
Status SplitFreeListAllocator<BlockType>::Init(ByteSpan region,
                                               size_t threshold) {
  Result<BlockType*> result = BlockType::Init(region);
  if (!result.ok()) {
    return result.status();
  }

  // Initially, the block list is a single free block.
  BlockType* block = *result;
  begin_ = block->UsableSpace();
  end_ = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(begin_) +
                                 block->InnerSize());
  first_free_ = block;
  last_free_ = block;

  threshold_ = threshold;
  return OkStatus();
}

template <typename BlockType>
void* SplitFreeListAllocator<BlockType>::DoAllocate(Layout layout) {
  if (begin_ == nullptr || layout.size() == 0) {
    return nullptr;
  }
  size_t size = layout.size();
  size_t alignment = std::max(layout.alignment(), BlockType::kAlignment);
  layout = Layout(size, alignment);
  return size < threshold_ ? DoAllocateSmall(layout) : DoAllocateLarge(layout);
}

template <typename BlockType>
void* SplitFreeListAllocator<BlockType>::DoAllocateSmall(Layout layout) {
  // Update the cached last free block.
  while (last_free_->Used() && first_free_ != last_free_) {
    last_free_ = last_free_->Prev();
  }
  // Search backwards for the first block that can hold this allocation.
  for (auto* block : ReverseRange(last_free_, first_free_)) {
    if (BlockType::AllocLast(block, layout.size(), layout.alignment()).ok()) {
      return block->UsableSpace();
    }
  }
  // No valid block found.
  return nullptr;
}

template <typename BlockType>
void* SplitFreeListAllocator<BlockType>::DoAllocateLarge(Layout layout) {
  // Update the cached first free block.
  while (first_free_->Used() && first_free_ != last_free_) {
    first_free_ = first_free_->Next();
  }
  // Search forwards for the first block that can hold this allocation.
  for (auto* block : Range(first_free_, last_free_)) {
    if (BlockType::AllocFirst(block, layout.size(), layout.alignment()).ok()) {
      // A new last free block may be split off the end of the allocated block.
      if (last_free_ <= block) {
        last_free_ = block->Last() ? block : block->Next();
      }
      return block->UsableSpace();
    }
  }
  // No valid block found.
  return nullptr;
}

template <typename BlockType>
void SplitFreeListAllocator<BlockType>::DoDeallocate(void* ptr, Layout) {
  // Do nothing if uninitialized or no memory block pointer.
  if (begin_ == nullptr || ptr < begin_ || end_ <= ptr) {
    return;
  }
  auto* block = BlockType::FromUsableSpace(static_cast<std::byte*>(ptr));
  block->CrashIfInvalid();

  // Free the block and merge it with its neighbors, if possible.
  BlockType::Free(block);

  // Update the first and/or last free block pointers.
  if (block < first_free_) {
    first_free_ = block;
  }
  if (block->Last() || last_free_ < block->Next()) {
    last_free_ = block;
  }
}

template <typename BlockType>
bool SplitFreeListAllocator<BlockType>::DoResize(void* ptr,
                                                 Layout layout,
                                                 size_t new_size) {
  // Fail to resize is uninitialized or invalid parameters.
  if (begin_ == nullptr || !DoQuery(ptr, layout).ok()) {
    return false;
  }

  // Ensure that this allocation came from this object.
  auto* block = BlockType::FromUsableSpace(static_cast<std::byte*>(ptr));
  block->CrashIfInvalid();

  bool next_is_first_free = !block->Last() && block->Next() == first_free_;
  bool next_is_last_free = !block->Last() && block->Next() == last_free_;
  if (!BlockType::Resize(block, new_size).ok()) {
    return false;
  }

  // The block after this one may have grown or shrunk.
  if (next_is_first_free) {
    first_free_ = block->Last() ? block : block->Next();
  }
  if (next_is_last_free) {
    last_free_ = block->Last() ? block : block->Next();
  }
  return true;
}

template <typename BlockType>
Result<Layout> SplitFreeListAllocator<BlockType>::DoGetLayout(
    const void* ptr) const {
  if (ptr < begin_ || end_ <= ptr) {
    return Status::NotFound();
  }
  const auto* block =
      BlockType::FromUsableSpace(static_cast<const std::byte*>(ptr));
  if (!block->IsValid()) {
    return Status::NotFound();
  }
  return block->GetLayout();
}

template <typename BlockType>
Status SplitFreeListAllocator<BlockType>::DoQuery(const void* ptr,
                                                  Layout) const {
  return (ptr < begin_ || end_ <= ptr) ? Status::OutOfRange() : OkStatus();
}

}  // namespace pw::allocator
