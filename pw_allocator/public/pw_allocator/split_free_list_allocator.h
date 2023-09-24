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

#include <cstddef>
#include <cstdint>
#include <optional>

#include "pw_allocator/allocator.h"
#include "pw_status/status.h"

namespace pw::allocator {

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
class SplitFreeListAllocator : public Allocator {
 public:
  /// Free memory blocks are tracked using a singly linked list. The free memory
  /// itself is used to for these structs, so the minimum size and alignment
  /// supported by this allocator is `sizeof(FreeBlock)`.
  ///
  /// Allocator callers should not need to access this type directly.
  struct FreeBlock {
    FreeBlock* next;
    size_t size;
  };

  constexpr SplitFreeListAllocator() = default;
  ~SplitFreeListAllocator() override;

  // Not copyable.
  SplitFreeListAllocator(const SplitFreeListAllocator&) = delete;
  SplitFreeListAllocator& operator=(const SplitFreeListAllocator&) = delete;

  /// Sets the memory region to be used by this allocator, and the threshold at
  /// which allocations are considerd "large" or "small". Large and small
  /// allocations return lower and higher addresses, respectively.
  ///
  /// @param[in]  base        Start of the memory region for this allocator.
  /// @param[in]  size        Length of the memory region for this allocator.
  /// @param[in]  threshold   Allocations of this size of larger are considered
  ///                         "large" and come from lower addresses.
  void Initialize(void* base, size_t size, size_t threshold);

 private:
  /// Adds the given block to the free list. The block must not be null.
  void AddBlock(FreeBlock* block);

  /// Removes the given block from the free list. The block must not be null.
  FreeBlock* RemoveBlock(FreeBlock* prev, FreeBlock* block);

  /// @copydoc Allocator::Dispatch
  Status DoQuery(const void* ptr, size_t size, size_t alignment) const override;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(size_t size, size_t alignment) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, size_t size, size_t alignment) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr,
                size_t old_size,
                size_t old_alignment,
                size_t new_size) override;

  uintptr_t addr_ = 0;
  size_t size_ = 0;
  FreeBlock* head_ = nullptr;
  size_t threshold_ = 0;
};

}  // namespace pw::allocator
