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
namespace internal {

AllocatorForTestImpl::~AllocatorForTestImpl() { PW_DCHECK(!initialized_); }

Status AllocatorForTestImpl::Init(ByteSpan bytes) {
  ResetParameters();
  initialized_ = true;
  if (auto status = allocator_.Init(bytes); !status.ok()) {
    return status;
  }
  tracker_.Init(allocator_, bytes.size());
  return OkStatus();
}

void AllocatorForTestImpl::Exhaust() {
  for (auto* block : allocator_.blocks()) {
    block->MarkUsed();
  }
}

void AllocatorForTestImpl::ResetParameters() {
  allocate_size_ = 0;
  deallocate_ptr_ = nullptr;
  deallocate_size_ = 0;
  resize_ptr_ = nullptr;
  resize_old_size_ = 0;
  resize_new_size_ = 0;
}

void AllocatorForTestImpl::Reset() {
  for (auto* block : allocator_.blocks()) {
    BlockType::Free(block);
  }
  ResetParameters();
  allocator_.Reset();
  initialized_ = false;
}

void* AllocatorForTestImpl::DoAllocate(Layout layout) {
  allocate_size_ = layout.size();
  return tracker_.Allocate(layout);
}

void AllocatorForTestImpl::DoDeallocate(void* ptr, Layout layout) {
  deallocate_ptr_ = ptr;
  deallocate_size_ = layout.size();
  return tracker_.Deallocate(ptr, layout);
}

bool AllocatorForTestImpl::DoResize(void* ptr, Layout layout, size_t new_size) {
  resize_ptr_ = ptr;
  resize_old_size_ = layout.size();
  resize_new_size_ = new_size;
  return tracker_.Resize(ptr, layout, new_size);
}

Result<Layout> AllocatorForTestImpl::DoGetLayout(const void* ptr) const {
  return tracker_.GetLayout(ptr);
}

Status AllocatorForTestImpl::DoQuery(const void* ptr, Layout layout) const {
  return tracker_.Query(ptr, layout);
}

}  // namespace internal
}  // namespace pw::allocator::test
