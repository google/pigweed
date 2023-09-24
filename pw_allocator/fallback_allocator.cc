
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

#include "pw_allocator/fallback_allocator.h"

#include "pw_assert/check.h"

namespace pw::allocator {

void FallbackAllocator::Initialize(Allocator& primary, Allocator& secondary) {
  primary_ = &primary;
  secondary_ = &secondary;
}

Status FallbackAllocator::DoQuery(const void* ptr,
                                  size_t size,
                                  size_t alignment) const {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  auto status = primary_->QueryUnchecked(ptr, size, alignment);
  return status.ok() ? status
                     : secondary_->QueryUnchecked(ptr, size, alignment);
}

void* FallbackAllocator::DoAllocate(size_t size, size_t alignment) {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  void* ptr = primary_->AllocateUnchecked(size, alignment);
  return ptr != nullptr ? ptr : secondary_->AllocateUnchecked(size, alignment);
}

void FallbackAllocator::DoDeallocate(void* ptr, size_t size, size_t alignment) {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  if (primary_->QueryUnchecked(ptr, size, alignment).ok()) {
    primary_->DeallocateUnchecked(ptr, size, alignment);
  } else {
    secondary_->DeallocateUnchecked(ptr, size, alignment);
  }
}

bool FallbackAllocator::DoResize(void* ptr,
                                 size_t old_size,
                                 size_t old_alignment,
                                 size_t new_size) {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  return primary_->QueryUnchecked(ptr, old_size, old_alignment).ok()
             ? primary_->ResizeUnchecked(ptr, old_size, old_alignment, new_size)
             : secondary_->ResizeUnchecked(
                   ptr, old_size, old_alignment, new_size);
}

}  // namespace pw::allocator
