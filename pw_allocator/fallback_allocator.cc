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

#include "pw_allocator/capability.h"
#include "pw_assert/check.h"

namespace pw::allocator {

FallbackAllocator::FallbackAllocator(Allocator& primary, Allocator& secondary)
    : Allocator(primary.capabilities() | secondary.capabilities()),
      primary_(primary),
      secondary_(secondary) {
  PW_CHECK(primary.HasCapability(Capability::kImplementsRecognizes));
}

void* FallbackAllocator::DoAllocate(Layout layout) {
  void* ptr = primary_.Allocate(layout);
  return ptr != nullptr ? ptr : secondary_.Allocate(layout);
}

void FallbackAllocator::DoDeallocate(void* ptr) {
  if (Recognizes(primary_, ptr)) {
    primary_.Deallocate(ptr);
  } else {
    secondary_.Deallocate(ptr);
  }
}

void FallbackAllocator::DoDeallocate(void* ptr, Layout) { DoDeallocate(ptr); }

bool FallbackAllocator::DoResize(void* ptr, size_t new_size) {
  return Recognizes(primary_, ptr) ? primary_.Resize(ptr, new_size)
                                   : secondary_.Resize(ptr, new_size);
}

size_t FallbackAllocator::DoGetAllocated() const {
  return primary_.GetAllocated() + secondary_.GetAllocated();
}

size_t FallbackAllocator::DoGetCapacity() const {
  return primary_.GetCapacity() + secondary_.GetCapacity();
}

Layout FallbackAllocator::DoGetLayout(LayoutType layout_type,
                                      const void* ptr) const {
  Layout layout = GetLayout(primary_, layout_type, ptr);
  return layout.size() == 0 ? GetLayout(secondary_, layout_type, ptr) : layout;
}

bool FallbackAllocator::DoRecognizes(const void* ptr) const {
  return Recognizes(primary_, ptr) || Recognizes(secondary_, ptr);
}

}  // namespace pw::allocator
