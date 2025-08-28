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

#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_result/result.h"
#include "pw_sync/borrow.h"

namespace pw::allocator {
namespace internal {

/// Generic base class for a GuardedAllocator.
///
/// This allocator detects heap overflows by inserting "guard" values before and
/// after the usable allocated memory regions provided to the caller. In order
/// to reserve space for these values and preserve alignment, it modifies the
/// requested memory layout to include a suffix and an alignment-preserving
/// prefix.
///
/// The resulting layout ensures that:
///   1. The usable space is proceeded by a prefix of at least two words.
///   2. The first word of the prefix is the size of the prefix.
///   3. The second-to-last word of the prefix is the size of the prefix.
///      This may overlap with the first word if the prefix is only two words.
///   4. The last word of the prefix is a guard value whose integrity can be
///      checked.
///   5. Usable space of at least `layout.size()` bytes and aligned on a
///      `layout.alignment()` boundary follows the prefix.
///   6. A suffix of one word follows the usable space. It is another guard
///      value.
///
/// Visually, this resembles:
///   [size, ...] | size | guard | usable_space... | guard
///
/// Since this base class is only concerned with adjusting layouts and sizes
/// and is agnostic to specific allocator and block details, all of its methods
/// are static.
class GenericGuardedAllocator : public Allocator {
 protected:
  constexpr explicit GenericGuardedAllocator(const Capabilities& capabilities)
      : Allocator(capabilities) {}

  /// Modifies a layout to include guard values.
  ///
  /// As this method does not touch allocatable memory, it is inherently
  /// thread-safe.
  static Layout AdjustLayout(Layout layout);

  /// Modifies an inner size parameter to account for the prefix and suffix.
  ///
  /// This is used by methods that deal with inner sizes and allocated blocks,
  /// e.g. `Resize`.
  static size_t AdjustSize(void* ptr, size_t inner_size);

  /// Takes a `ptr` to a guarded region and returns the pointer to the original
  /// allocation.
  static void* GetOriginal(void* ptr);

  /// Adds a prefix a the start of the allocation given by `ptr` and `size`,
  /// and returns a a pointer to the guarded region following it.
  static void* AddPrefix(void* ptr, size_t alignment);

  /// Adds a suffix a the end of the allocation given by `ptr` and `size`.
  static void AddSuffix(void* ptr, size_t size);

  /// Returns whether the allocation given by `ptr` and `size` has a valid
  /// prefix and suffix.
  static bool CheckPrefixAndSuffix(void* ptr, size_t size);
};

}  // namespace internal

/// @submodule{pw_allocator,forwarding}

/// GuardedAllocator that can detect heap overflows in a thread-safe manner.
///
/// This class takes a `BlockAllocator` and manages concurrent access to it.
/// This allows a background thread to validate allocations using one of two
/// key methods:
///
/// * `ValidateOne` will validate a single block each time it is called.
///   Successive calls will eventually iterate over all blocks.
/// * `ValidateAll` vill validate all current blocks. Other allocator methods
///   will block until the validation is complete.
///
/// Both methods can be called explicitly, or used to create a thread that
/// periodically validates, e.g.
///
/// @code{.cpp}
/// Thread thread(options, [&guarded_allocator]() {
///   while (true) {
///     guarded_allocator.ValidateOne();
///     pw::this_thread::sleep_for(500ms);
///   }
/// });
/// @endcode
///
/// Note that while this allocator wraps a `BlockAllocator` it is NOT a
/// block allocator itself. In particular, pointers allocated from this
/// allocator MUST NOT be passed to methods like `BlockType::FromUsableSpace`.
template <typename BlockAllocatorType, typename LockType = NoSync>
class GuardedAllocator : public internal::GenericGuardedAllocator {
 private:
  using Base = internal::GenericGuardedAllocator;

 public:
  using BlockType = typename BlockAllocatorType::BlockType;

  constexpr explicit GuardedAllocator(BlockAllocatorType& allocator)
      : Base(allocator.capabilities()), borrowable_(allocator, lock_) {
    block_ = *(allocator.blocks().end());
  }

  /// Checks for heap overflows in an allocation.
  ///
  /// This method may be called explicitly, or repeatedly from a background
  /// thread. The individual allocation being checked is not specified, but
  /// repeated calls will eventually iterate over all blocks.
  ///
  /// @returns A pointer to the memory allocation with a corrupted prefix and/or
  /// suffix.
  void* ValidateOne();

  /// Checks for heap overflows in all current allocations.
  ///
  /// This method may be called explicitly, or repeatedly from a background
  /// thread. Other allocator methods will block until the validation is
  /// complete.
  ///
  /// @returns A pointer to the memory allocation with a corrupted prefix and/or
  /// suffix.
  void* ValidateAll();

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

  LockType lock_;
  sync::Borrowable<BlockAllocatorType, LockType> borrowable_;

  // Ideally, this would be annotated as being guarded `lock_`. However, the
  // lock is always acquired via `borrowable_`, which does not support thread
  // safety analysis.
  BlockType* block_;
};

/// @}

// Template method implementations.

template <typename BlockAllocatorType, typename LockType>
void* GuardedAllocator<BlockAllocatorType, LockType>::ValidateOne() {
  auto allocator = borrowable_.acquire();

  // Find the bounds of the block range.
  auto range = allocator->blocks();
  BlockType* begin = *(range.begin());
  BlockType* end = *(range.end());

  // Ensure there is at least one block.
  if (begin == end) {
    return nullptr;
  }

  // Ensure we are starting from a block.
  if (block_ == nullptr || block_ == end) {
    block_ = begin;
  }

  // Find the next used block.
  BlockType* prev = block_;
  while (!block_->Used()) {
    BlockType* next = block_->Next();
    if (next == end) {
      // Loop around.
      next = begin;
    }
    if (next == prev) {
      // All blocks are free.
      return nullptr;
    }
    block_ = next;
  }

  // Validate the block.
  void* ptr = block_->UsableSpace();
  size_t size = block_->InnerSize();
  return Base::CheckPrefixAndSuffix(ptr, size) ? nullptr : ptr;
}

template <typename BlockAllocatorType, typename LockType>
void* GuardedAllocator<BlockAllocatorType, LockType>::ValidateAll() {
  auto allocator = borrowable_.acquire();
  for (BlockType* block : allocator->blocks()) {
    if (!block->Used()) {
      continue;
    }
    void* ptr = block->UsableSpace();
    size_t size = block->InnerSize();
    if (!Base::CheckPrefixAndSuffix(ptr, size)) {
      return ptr;
    }
  }
  return nullptr;
}

template <typename BlockAllocatorType, typename LockType>
void* GuardedAllocator<BlockAllocatorType, LockType>::DoAllocate(
    Layout layout) {
  layout = Base::AdjustLayout(layout);
  auto allocator = borrowable_.acquire();
  void* ptr = allocator->Allocate(layout);
  if (ptr == nullptr) {
    return nullptr;
  }
  auto* block = BlockType::FromUsableSpace(ptr);

  // Bytes may be shifted to the previous block.
  BlockType* prev = block->Prev();
  if (prev != nullptr && prev->Used()) {
    Base::AddSuffix(prev->UsableSpace(), prev->InnerSize());
  }

  Base::AddSuffix(block->UsableSpace(), block->InnerSize());
  return Base::AddPrefix(ptr, layout.alignment());
}

template <typename BlockAllocatorType, typename LockType>
void GuardedAllocator<BlockAllocatorType, LockType>::DoDeallocate(void* ptr) {
  ptr = Base::GetOriginal(ptr);

  // This block and or the next one may be merged on deallocation.
  auto allocator = borrowable_.acquire();
  auto* block = BlockType::FromUsableSpace(ptr);
  BlockType* prev = block->Prev();
  if (block_ == block || block_ == block->Next()) {
    block_ = prev;
  }
  allocator->Deallocate(ptr);

  // Bytes may have been shifted from the previous block.
  if (prev != nullptr && prev->Used()) {
    Base::AddSuffix(prev->UsableSpace(), prev->InnerSize());
  }
}

template <typename BlockAllocatorType, typename LockType>
bool GuardedAllocator<BlockAllocatorType, LockType>::DoResize(void* ptr,
                                                              size_t new_size) {
  ptr = Base::GetOriginal(ptr);
  new_size = Base::AdjustSize(ptr, new_size);

  // The next block may be recreated on resizing.
  auto allocator = borrowable_.acquire();
  auto* block = BlockType::FromUsableSpace(ptr);
  if (block_ == block->Next()) {
    block_ = block;
  }
  if (!allocator->Resize(ptr, new_size)) {
    return false;
  }
  Base::AddSuffix(block->UsableSpace(), block->InnerSize());
  return true;
}

template <typename BlockAllocatorType, typename LockType>
Result<Layout> GuardedAllocator<BlockAllocatorType, LockType>::DoGetInfo(
    InfoType info_type, const void* ptr) const {
  ptr = const_cast<const void*>(Base::GetOriginal(const_cast<void*>(ptr)));
  auto allocator = borrowable_.acquire();
  return BlockAllocatorType::GetInfo(*allocator, info_type, ptr);
}

}  // namespace pw::allocator
