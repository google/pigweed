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

#include "pw_allocator/split_free_list_allocator.h"

#include <algorithm>

#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"

namespace pw::allocator {

static_assert(sizeof(size_t) == sizeof(uintptr_t), "platform not supported");

using FreeBlock = SplitFreeListAllocator::FreeBlock;

// Public methods.

SplitFreeListAllocator::~SplitFreeListAllocator() {
  // All memory must be returned before the allocator goes out of scope.
  if (addr_ != 0) {
    PW_CHECK(addr_ == reinterpret_cast<uintptr_t>(head_));
    PW_CHECK(head_->next == nullptr);
    PW_CHECK(head_->size == size_);
  }
}

void SplitFreeListAllocator::Initialize(void* base,
                                        size_t size,
                                        size_t threshold) {
  PW_CHECK(base != nullptr);
  auto addr = reinterpret_cast<uintptr_t>(base);

  // See `Normalize` below. All addresses, including the start and end of the
  // overall memory region, must be a multiple of and aligned to
  // `sizeof(FreeBlock)`.
  addr_ = AlignUp(addr, sizeof(FreeBlock));
  PW_CHECK(sizeof(FreeBlock) <= size, "size underflow on alignment");
  size_ = AlignDown(size - (addr_ - addr), sizeof(FreeBlock));
  PW_CHECK(sizeof(FreeBlock) <= size_, "region is smaller than a single block");

  head_ = reinterpret_cast<FreeBlock*>(addr_);
  head_->next = nullptr;
  head_->size = size_;
  threshold_ = threshold;
}

// Private methods.

namespace {

/// Adjust the layout if necessary to match `SplitFreeListAllocator`'s minimums.
///
/// This functions will modify `size` and `alignment` to represent a memory
/// region that is a multiple of `sizeof(FreeBlock)`, aligned on
/// `sizeof(FreeBlock)` boundaries.
///
/// The use of such minimum sizes and alignments eliminates several conditions
/// and edge cases that would need to checked and addressed if more granular
/// sizes and alignments were used. It also greatly simplifies ensuring that any
/// fragments can hold a `FreeBlock` as well as reconstructing the `FreeBlock`
/// from a pointer and `Layout` in `Deallocate`.
///
/// These simplifications allow de/allocation to be quicker, at the potential
/// cost of a few bytes wasted for small and/or less strictly aligned
/// allocations.
void Normalize(size_t& size, size_t& alignment) {
  alignment = std::max(alignment, sizeof(FreeBlock));
  size = AlignUp(std::max(size, sizeof(FreeBlock)), alignment);
}

/// Stores a `FreeBlock` representing a block of the given `size` at
/// `ptr` + `offset`, and returns it.
FreeBlock* CreateBlock(void* ptr, size_t size, size_t offset = 0) {
  auto addr = reinterpret_cast<uintptr_t>(ptr) + offset;
  auto* block = reinterpret_cast<FreeBlock*>(addr);
  block->next = nullptr;
  block->size = size;
  return block;
}

/// Returns true if `prev` + `offset` equals `next`.
bool IsAdjacent(void* prev, size_t offset, void* next) {
  return reinterpret_cast<uintptr_t>(prev) + offset ==
         reinterpret_cast<uintptr_t>(next);
}

/// Reduces the size of a block and creates and returns a new block representing
/// the difference.
///
/// The original block must have room for both resulting `FreeBlock`s.
///
/// This function assumes `prev` IS on a free list.
FreeBlock* SplitBlock(FreeBlock* prev, size_t offset) {
  PW_DCHECK(sizeof(FreeBlock) <= offset);
  PW_DCHECK(offset + sizeof(FreeBlock) <= prev->size);
  FreeBlock* next = CreateBlock(prev, prev->size - offset, offset);
  next->next = prev->next;
  prev->size = offset;
  prev->next = next;
  return next;
}

/// Combines two blocks into one and returns it.
///
/// `prev` and `next` MUJST NOT be null.

/// This function assumes `prev` and `next` ARE NOT on a free list.
FreeBlock* MergeBlocks(FreeBlock* prev, FreeBlock* next) {
  PW_DCHECK(prev != nullptr);
  PW_DCHECK(next != nullptr);
  prev->size += next->size;
  return prev;
}

}  // namespace

void SplitFreeListAllocator::AddBlock(FreeBlock* block) {
  PW_DCHECK(addr_ != 0);
  PW_DCHECK(block != nullptr);
  block->next = head_;
  head_ = block;
}

SplitFreeListAllocator::FreeBlock* SplitFreeListAllocator::RemoveBlock(
    FreeBlock* prev, FreeBlock* block) {
  PW_DCHECK(addr_ != 0);
  PW_DCHECK(block != nullptr);
  if (block == head_) {
    head_ = block->next;
  } else {
    prev->next = block->next;
  }
  return block;
}

Status SplitFreeListAllocator::DoQuery(const void* ptr,
                                       size_t size,
                                       size_t alignment) const {
  PW_DCHECK(addr_ != 0);
  if (ptr == nullptr || size == 0) {
    return Status::OutOfRange();
  }
  Normalize(size, alignment);
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  if (addr + size <= addr || addr < addr_ || addr_ + size_ < addr + size) {
    return Status::OutOfRange();
  }
  return OkStatus();
}

void* SplitFreeListAllocator::DoAllocate(size_t size, size_t alignment) {
  PW_DCHECK(addr_ != 0);
  if (head_ == nullptr || size == 0 || size_ < size) {
    return nullptr;
  }
  Normalize(size, alignment);

  // Blocks over and under the threshold are allocated from lower and higher
  // addresses, respectively.
  bool from_lower = threshold_ <= size;
  FreeBlock* prev = nullptr;
  FreeBlock* block = nullptr;
  size_t offset = 0;
  for (FreeBlock *previous = nullptr, *current = head_; current != nullptr;
       previous = current, current = current->next) {
    if (current->size < size) {
      continue;
    }
    // Fragment large requests from the start of the block, and small requests
    // from the back. Verify the aligned offsets are still within the block.
    uintptr_t current_start = reinterpret_cast<uintptr_t>(current);
    uintptr_t current_end = current_start + current->size;
    uintptr_t addr = from_lower ? AlignUp(current_start, alignment)
                                : AlignDown(current_end - size, alignment);
    if (addr < current_start || current_end < addr + size) {
      continue;
    }
    // Update `prev` and `block` if the current block is earlier or later and we
    // want blocks with lower or higher address, respectively.
    if (block == nullptr || (current < block) == from_lower) {
      prev = previous;
      block = current;
      offset = addr - current_start;
    }
  }
  if (block == nullptr) {
    return nullptr;
  }
  if (offset != 0) {
    prev = block;
    block = SplitBlock(block, offset);
  }
  if (size < block->size) {
    SplitBlock(block, size);
  }
  return RemoveBlock(prev, block);
}

void SplitFreeListAllocator::DoDeallocate(void* ptr,
                                          size_t size,
                                          size_t alignment) {
  PW_DCHECK(addr_ != 0);

  // Do nothing if no memory block pointer.
  if (ptr == nullptr) {
    return;
  }

  // Ensure that this allocation came from this object.
  PW_DCHECK(DoQuery(ptr, size, alignment).ok());

  Normalize(size, alignment);
  FreeBlock* block = CreateBlock(ptr, size);
  for (FreeBlock *previous = nullptr, *current = head_; current != nullptr;
       current = current->next) {
    if (IsAdjacent(current, current->size, block)) {
      // Block precedes block being freed. Remove from list and merge.
      block = MergeBlocks(RemoveBlock(previous, current), block);
    } else if (IsAdjacent(block, block->size, current)) {
      // Block follows block being freed. Remove from list and merge.
      block = MergeBlocks(block, RemoveBlock(previous, current));
    } else {
      previous = current;
    }
  }

  // Add released block to the free list.
  AddBlock(block);
}

bool SplitFreeListAllocator::DoResize(void* ptr,
                                      size_t old_size,
                                      size_t old_alignment,
                                      size_t new_size) {
  PW_DCHECK(addr_ != 0);

  if (ptr == nullptr || old_size == 0 || new_size == 0) {
    return false;
  }

  // Ensure that this allocation came from this object.
  PW_DCHECK(DoQuery(ptr, old_size, old_alignment).ok());

  // Do nothing if new size equals current size.
  Normalize(old_size, old_alignment);
  Normalize(new_size, old_alignment);
  if (old_size == new_size) {
    return true;
  }
  bool growing = old_size < new_size;
  size_t diff = growing ? new_size - old_size : old_size - new_size;
  // Try to find a free block that follows this one.
  FreeBlock* prev = nullptr;
  FreeBlock* next = head_;
  while (next != nullptr && !IsAdjacent(ptr, old_size, next)) {
    prev = next;
    next = next->next;
  }
  if (growing) {
    if (next == nullptr || next->size < diff) {
      // No free neighbor that is large enough. Must reallocate.
      return false;
    }
    // Split the next block and remove the portion to be returned.
    if (diff != next->size) {
      SplitBlock(next, diff);
    }
    RemoveBlock(prev, next);
  } else /* !growing*/ {
    if (next == nullptr) {
      // Create a new block for the extra space and add it.
      next = CreateBlock(ptr, diff, new_size);
    } else {
      // Merge the extra space with the next block.
      RemoveBlock(prev, next);
      prev = CreateBlock(ptr, diff, new_size);
      next = MergeBlocks(prev, next);
    }
    AddBlock(next);
  }
  return true;
}

}  // namespace pw::allocator
