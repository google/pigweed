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

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"

namespace pw::allocator {

// DOCSTAG: [pw_allocator_examples_simple_allocator]
/// Simple allocator that uses a list of `Block`s.
class SimpleAllocator : public Allocator {
 public:
  using Block = pw::allocator::Block<>;
  using Range = typename Block::Range;

  constexpr SimpleAllocator() = default;

  /// Initialize this allocator to allocate memory from `region`.
  Status Init(ByteSpan region) {
    auto result = Block::Init(region);
    if (result.ok()) {
      blocks_ = *result;
    }
    return result.status();
  }

  /// Return the range of blocks for this allocator.
  Range blocks() { return Range(blocks_); }

 private:
  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, size_t, size_t) const override {
    for (auto* block : Range(blocks_)) {
      if (block->UsableSpace() == ptr) {
        return OkStatus();
      }
    }
    return Status::OutOfRange();
  }

  /// @copydoc Allocator::Allocate
  void* DoAllocate(size_t size, size_t alignment) override {
    for (auto* block : Range(blocks_)) {
      if (Block::AllocFirst(block, size, alignment).ok()) {
        return block->UsableSpace();
      }
    }
    return nullptr;
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, size_t, size_t) override {
    if (ptr == nullptr) {
      return;
    }
    auto* bytes = static_cast<std::byte*>(ptr);
    Block* block = Block::FromUsableSpace(bytes);
    Block::Free(block);
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t, size_t, size_t new_size) override {
    if (ptr == nullptr) {
      return false;
    }
    auto* bytes = static_cast<std::byte*>(ptr);
    Block* block = Block::FromUsableSpace(bytes);
    return Block::Resize(block, new_size).ok();
  }

  Block* blocks_ = nullptr;
};
// DOCSTAG: [pw_allocator_examples_simple_allocator]

}  // namespace pw::allocator
