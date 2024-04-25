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
  PW_CHECK(primary.HasCapability(Capability::kImplementsQuery));
}

void* FallbackAllocator::DoAllocate(Layout layout) {
  void* ptr = primary_.Allocate(layout);
  return ptr != nullptr ? ptr : secondary_.Allocate(layout);
}

void FallbackAllocator::DoDeallocate(void* ptr) {
  if (Query(primary_, ptr).ok()) {
    primary_.Deallocate(ptr);
  } else {
    secondary_.Deallocate(ptr);
  }
}

void FallbackAllocator::DoDeallocate(void* ptr, Layout) { DoDeallocate(ptr); }

bool FallbackAllocator::DoResize(void* ptr, size_t new_size) {
  return Query(primary_, ptr).ok() ? primary_.Resize(ptr, new_size)
                                   : secondary_.Resize(ptr, new_size);
}

StatusWithSize FallbackAllocator::DoGetCapacity() const {
  StatusWithSize primary = primary_.GetCapacity();
  if (!primary.ok()) {
    return primary;
  }
  StatusWithSize secondary = secondary_.GetCapacity();
  if (!secondary.ok()) {
    return secondary;
  }
  return StatusWithSize(primary.size() + secondary.size());
}

Result<Layout> FallbackAllocator::DoGetRequestedLayout(const void* ptr) const {
  Result<Layout> primary = GetRequestedLayout(primary_, ptr);
  return primary.ok() ? primary
                      : CombineResults(primary.status(),
                                       GetRequestedLayout(secondary_, ptr));
}

Result<Layout> FallbackAllocator::DoGetUsableLayout(const void* ptr) const {
  Result<Layout> primary = GetUsableLayout(primary_, ptr);
  return primary.ok() ? primary
                      : CombineResults(primary.status(),
                                       GetUsableLayout(secondary_, ptr));
}

Result<Layout> FallbackAllocator::DoGetAllocatedLayout(const void* ptr) const {
  Result<Layout> primary = GetAllocatedLayout(primary_, ptr);
  return primary.ok() ? primary
                      : CombineResults(primary.status(),
                                       GetAllocatedLayout(secondary_, ptr));
}

Status FallbackAllocator::DoQuery(const void* ptr) const {
  Status status = Query(primary_, ptr);
  return status.ok() ? status : Query(secondary_, ptr);
}

/// Combines a secondary result with a primary, non-ok status.
Result<Layout> FallbackAllocator::CombineResults(
    Status primary, const Result<Layout>& secondary) const {
  if (!secondary.ok() && !primary.IsUnimplemented()) {
    return primary;
  }
  return secondary;
}

}  // namespace pw::allocator
