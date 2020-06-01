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

#include "pw_allocator/block.h"
#include "pw_allocator/freelist.h"
#include "pw_span/span.h"

namespace pw::allocator {

class FreeListHeap {
 public:
  FreeListHeap(span<std::byte> region, FreeList& freelist);

  void* Allocate(size_t size);
  void Free(void* ptr);
  void* Realloc(void* ptr, size_t size);
  void* Calloc(size_t num, size_t size);

 private:
  span<std::byte> BlockToSpan(Block* block) {
    return span<std::byte>(block->UsableSpace(), block->InnerSize());
  }

  span<std::byte> region_;
  FreeList& freelist_;
};

template <size_t N = 6>
class FreeListHeapBuffer {
 public:
  static constexpr std::array<size_t, N> defaultBuckets{
      16, 32, 64, 128, 256, 512};

  FreeListHeapBuffer(span<std::byte> region)
      : freelist_(defaultBuckets), heap_(region, freelist_) {}

  void* Allocate(size_t size) { return heap_.Allocate(size); }
  void Free(void* ptr) { heap_.Free(ptr); }
  void* Realloc(void* ptr, size_t size) { return heap_.Realloc(ptr, size); }
  void* Calloc(size_t num, size_t size) { return heap_.Calloc(num, size); }

 private:
  FreeListBuffer<N> freelist_;
  FreeListHeap heap_;
};

}  // namespace pw::allocator
