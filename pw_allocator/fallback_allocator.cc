
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

Status FallbackAllocator::DoQuery(const void* ptr, Layout layout) const {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  auto status = primary_->Query(ptr, layout);
  return status.ok() ? status : secondary_->Query(ptr, layout);
}

void* FallbackAllocator::DoAllocate(Layout layout) {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  void* ptr = primary_->Allocate(layout);
  return ptr != nullptr ? ptr : secondary_->Allocate(layout);
}

void FallbackAllocator::DoDeallocate(void* ptr, Layout layout) {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  if (primary_->Query(ptr, layout).ok()) {
    primary_->Deallocate(ptr, layout);
  } else {
    secondary_->Deallocate(ptr, layout);
  }
}

bool FallbackAllocator::DoResize(void* ptr, Layout layout, size_t new_size) {
  PW_DCHECK(primary_ != nullptr && secondary_ != nullptr);
  return primary_->Query(ptr, layout).ok()
             ? primary_->Resize(ptr, layout, new_size)
             : secondary_->Resize(ptr, layout, new_size);
}

}  // namespace pw::allocator
