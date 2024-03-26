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

#include "pw_allocator/testing.h"

#include "pw_assert/check.h"

namespace pw::allocator::internal {

void* AllocatorForTestImpl::DoAllocate(Layout layout) {
  params_.allocate_size = layout.size();
  return allocator_.Allocate(layout);
}

void AllocatorForTestImpl::DoDeallocate(void* ptr, Layout layout) {
  params_.deallocate_ptr = ptr;
  params_.deallocate_size = layout.size();
  return allocator_.Deallocate(ptr, layout);
}

bool AllocatorForTestImpl::DoResize(void* ptr, Layout layout, size_t new_size) {
  params_.resize_ptr = ptr;
  params_.resize_old_size = layout.size();
  params_.resize_new_size = new_size;
  return allocator_.Resize(ptr, layout, new_size);
}

StatusWithSize AllocatorForTestImpl::DoGetCapacity() const {
  return allocator_.GetCapacity();
}

Result<Layout> AllocatorForTestImpl::DoGetRequestedLayout(
    const void* ptr) const {
  return allocator_.GetRequestedLayout(ptr);
}

Result<Layout> AllocatorForTestImpl::DoGetUsableLayout(const void* ptr) const {
  return allocator_.GetUsableLayout(ptr);
}

Result<Layout> AllocatorForTestImpl::DoGetAllocatedLayout(
    const void* ptr) const {
  return allocator_.GetAllocatedLayout(ptr);
}

Status AllocatorForTestImpl::DoQuery(const void* ptr, Layout layout) const {
  return allocator_.Query(ptr, layout);
}

}  // namespace pw::allocator::internal
