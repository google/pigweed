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

#include <array>
#include <cstddef>

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_bytes/span.h"

namespace pw::allocator::test {

/// Fake memory allocator for testing.
///
/// This allocator can return a fixed number of allocations made using an
/// internal buffer. It records the most recent parameters passed to the
/// `Allocator` interface methods, and returns them via accessors.
class FakeAllocator : public Allocator {
 public:
  Status Initialize(ByteSpan buffer);

  size_t allocate_size() const { return allocate_size_; }

  void* deallocate_ptr() const { return deallocate_ptr_; }
  size_t deallocate_size() const { return deallocate_size_; }

  void* resize_ptr() const { return resize_ptr_; }
  size_t resize_old_size() const { return resize_old_size_; }
  size_t resize_new_size() const { return resize_new_size_; }

  void ResetParameters();

 private:
  /// @copydoc Allocator::Query
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

  Block* head_ = nullptr;
  size_t allocate_size_;
  void* deallocate_ptr_;
  size_t deallocate_size_;
  void* resize_ptr_;
  size_t resize_old_size_;
  size_t resize_new_size_;
};

}  // namespace pw::allocator::test
