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

#include "pw_allocator/allocator_as_pool.h"

namespace pw::allocator {

AllocatorAsPool::AllocatorAsPool(Allocator& allocator, const Layout& layout)
    : Pool(allocator.capabilities(), layout), allocator_(allocator) {}

void* AllocatorAsPool::DoAllocate() { return allocator_.Allocate(layout()); }

void AllocatorAsPool::DoDeallocate(void* ptr) { allocator_.Deallocate(ptr); }

size_t AllocatorAsPool::DoGetCapacity() const {
  return allocator_.GetCapacity();
}

Layout AllocatorAsPool::DoGetLayout(LayoutType layout_type,
                                    const void* ptr) const {
  return GetLayout(allocator_, layout_type, ptr);
}

bool AllocatorAsPool::DoRecognizes(const void* ptr) const {
  return Recognizes(allocator_, ptr);
}

}  // namespace pw::allocator
