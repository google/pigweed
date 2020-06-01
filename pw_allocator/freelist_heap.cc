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

#include "pw_allocator/freelist_heap.h"

#include <cstring>

namespace pw::allocator {

FreeListHeap::FreeListHeap(span<std::byte> region, FreeList& freelist)
    : freelist_(freelist) {
  Block* block;
  Block::Init(region, &block);

  freelist_.AddChunk(BlockToSpan(block));

  region_ = region;
}

void* FreeListHeap::Allocate(size_t size) {
  // Find a chunk in the freelist. Split it if needed, then return
  auto chunk = freelist_.FindChunk(size);

  if (chunk.data() == nullptr) {
    return nullptr;
  }
  freelist_.RemoveChunk(chunk);

  Block* chunk_block = Block::FromUsableSpace(chunk.data());

  // Split that chunk. If there's a leftover chunk, add it to the freelist
  Block* leftover;
  auto status = chunk_block->Split(size, &leftover);
  if (status == PW_STATUS_OK) {
    freelist_.AddChunk(BlockToSpan(leftover));
  }

  chunk_block->MarkUsed();

  return chunk_block->UsableSpace();
}

void FreeListHeap::Free(void* ptr) {
  std::byte* bytes = reinterpret_cast<std::byte*>(ptr);

  if (bytes < region_.data() || bytes >= region_.data() + region_.size()) {
    return;
  }

  Block* chunk_block = Block::FromUsableSpace(bytes);
  // Ensure that the block is in-use
  if (!chunk_block->Used()) {
    return;
  }
  chunk_block->MarkFree();
  // Can we combine with the left or right blocks?
  Block* prev = chunk_block->PrevBlock();
  Block* next = nullptr;

  if (!chunk_block->Last()) {
    next = chunk_block->NextBlock();
  }

  if (prev != nullptr && !prev->Used()) {
    // Remove from freelist and merge
    freelist_.RemoveChunk(BlockToSpan(prev));
    chunk_block->MergePrev();

    // chunk_block is now invalid; prev now encompasses it.
    chunk_block = prev;
  }

  if (next != nullptr && !next->Used()) {
    freelist_.RemoveChunk(BlockToSpan(next));
    chunk_block->MergeNext();
  }
  // Add back to the freelist
  freelist_.AddChunk(BlockToSpan(chunk_block));
}

// Follows constract of the C standard realloc() function
// If ptr is free'd, will return nullptr.
void* FreeListHeap::Realloc(void* ptr, size_t size) {
  if (size == 0) {
    Free(ptr);
    return nullptr;
  }

  // If the pointer is nullptr, allocate a new memory.
  if (ptr == nullptr) {
    return Allocate(size);
  }

  std::byte* bytes = reinterpret_cast<std::byte*>(ptr);

  // TODO(chenghanzh): Enhance with debug information for out-of-range and more.
  if (bytes < region_.data() || bytes >= region_.data() + region_.size()) {
    return nullptr;
  }

  Block* chunk_block = Block::FromUsableSpace(bytes);
  if (!chunk_block->Used()) {
    return nullptr;
  }
  size_t old_size = chunk_block->InnerSize();

  // Do nothing and return ptr if the required memory size is smaller than
  // the current size.
  // TODO: Currently do not support shrink of memory chunk.
  if (old_size >= size) {
    return ptr;
  }

  void* new_ptr = Allocate(size);
  // Don't invalidate ptr if Allocate(size) fails to initilize the memory.
  if (new_ptr == nullptr) {
    return nullptr;
  }
  memcpy(new_ptr, ptr, old_size);

  Free(ptr);
  return new_ptr;
}

void* FreeListHeap::Calloc(size_t num, size_t size) {
  void* ptr = Allocate(num * size);
  if (ptr != nullptr) {
    memset(ptr, 0, num * size);
  }
  return ptr;
}

}  // namespace pw::allocator
