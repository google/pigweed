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

#include "pw_allocator/bump_allocator.h"

#include "pw_allocator/buffer.h"

namespace pw::allocator {

BumpAllocator::BumpAllocator(ByteSpan region)
    : Allocator(kCapabilities), remaining_(region) {}

BumpAllocator::~BumpAllocator() {
  if (owned_ != nullptr) {
    owned_->Destroy();
  }
}

void* BumpAllocator::DoAllocate(Layout layout) {
  Result<ByteSpan> region = GetAlignedSubspan(remaining_, layout.alignment());
  if (!region.ok() || region->size() < layout.size()) {
    return nullptr;
  }
  remaining_ = region->subspan(layout.size());
  return region->data();
}

void BumpAllocator::DoDeallocate(void*) {}

}  // namespace pw::allocator
