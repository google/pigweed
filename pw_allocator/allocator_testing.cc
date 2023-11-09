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

#include "pw_allocator/allocator_testing.h"

#include "pw_assert/check.h"

namespace pw::allocator::test {

AllocatorForTest::~AllocatorForTest() {
  for (auto* block : allocator_.blocks()) {
    PW_DCHECK(
        !block->Used(),
        "The block at %p was still in use when its allocator was "
        "destroyed. All memory allocated by an allocator must be released "
        "before the allocator goes out of scope.",
        static_cast<void*>(block));
  }
}

Status AllocatorForTest::Init(ByteSpan bytes) {
  ResetParameters();
  return allocator_.Init(bytes);
}

void AllocatorForTest::Exhaust() {
  for (auto* block : allocator_.blocks()) {
    block->MarkUsed();
  }
}

void AllocatorForTest::ResetParameters() {
  allocate_size_ = 0;
  deallocate_ptr_ = nullptr;
  deallocate_size_ = 0;
  resize_ptr_ = nullptr;
  resize_old_size_ = 0;
  resize_new_size_ = 0;
}

void AllocatorForTest::DeallocateAll() {
  for (auto* block : allocator_.blocks()) {
    BlockType::Free(block);
  }
  ResetParameters();
}

Status AllocatorForTest::DoQuery(const void* ptr, Layout layout) const {
  return allocator_.Query(ptr, layout);
}

void* AllocatorForTest::DoAllocate(Layout layout) {
  allocate_size_ = layout.size();
  return allocator_.Allocate(layout);
}

void AllocatorForTest::DoDeallocate(void* ptr, Layout layout) {
  deallocate_ptr_ = ptr;
  deallocate_size_ = layout.size();
  return allocator_.Deallocate(ptr, layout);
}

bool AllocatorForTest::DoResize(void* ptr, Layout layout, size_t new_size) {
  resize_ptr_ = ptr;
  resize_old_size_ = layout.size();
  resize_new_size_ = new_size;
  return allocator_.Resize(ptr, layout, new_size);
}

}  // namespace pw::allocator::test
