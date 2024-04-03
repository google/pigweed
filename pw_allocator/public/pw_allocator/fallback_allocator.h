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

#include <cstddef>

#include "pw_allocator/allocator.h"
#include "pw_allocator/capability.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// This class simply dispatches between a primary and secondary allocator. Any
/// attempt to allocate memory will first be handled by the primary allocator.
/// If it cannot allocate memory, e.g. because it is out of memory, the
/// secondary alloator will try to allocate memory instead.
class FallbackAllocator : public Allocator {
 public:
  /// Constructor.
  ///
  /// @param[in]  primary     Allocator tried first. Must implement `Query`.
  /// @param[in]  secondary   Allocator tried if `primary` fails a request.
  FallbackAllocator(Allocator& primary, Allocator& secondary)
      : Allocator(primary.capabilities() | secondary.capabilities()),
        primary_(primary),
        secondary_(secondary) {
    PW_ASSERT(primary.HasCapability(Capability::kImplementsQuery));
  }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    void* ptr = primary_.Allocate(layout);
    return ptr != nullptr ? ptr : secondary_.Allocate(layout);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override {
    if (Query(primary_, ptr).ok()) {
      primary_.Deallocate(ptr);
    } else {
      secondary_.Deallocate(ptr);
    }
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override { DoDeallocate(ptr); }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override {
    return Query(primary_, ptr).ok() ? primary_.Resize(ptr, new_size)
                                     : secondary_.Resize(ptr, new_size);
  }

  /// @copydoc Allocator::GetCapacity
  StatusWithSize DoGetCapacity() const override {
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

  /// @copydoc Allocator::GetRequestedLayout
  Result<Layout> DoGetRequestedLayout(const void* ptr) const override {
    Result<Layout> primary = GetRequestedLayout(primary_, ptr);
    return primary.ok() ? primary
                        : CombineResults(primary.status(),
                                         GetRequestedLayout(secondary_, ptr));
  }

  /// @copydoc Allocator::GetUsableLayout
  Result<Layout> DoGetUsableLayout(const void* ptr) const override {
    Result<Layout> primary = GetUsableLayout(primary_, ptr);
    return primary.ok() ? primary
                        : CombineResults(primary.status(),
                                         GetUsableLayout(secondary_, ptr));
  }

  /// @copydoc Allocator::GetAllocatedLayout
  Result<Layout> DoGetAllocatedLayout(const void* ptr) const override {
    Result<Layout> primary = GetAllocatedLayout(primary_, ptr);
    return primary.ok() ? primary
                        : CombineResults(primary.status(),
                                         GetAllocatedLayout(secondary_, ptr));
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr) const override {
    Status status = Query(primary_, ptr);
    return status.ok() ? status : Query(secondary_, ptr);
  }

  /// Combines a secondary result with a primary, non-ok status.
  Result<Layout> CombineResults(Status primary,
                                const Result<Layout>& secondary) const {
    if (!secondary.ok() && !primary.IsUnimplemented()) {
      return primary;
    }
    return secondary;
  }

  Allocator& primary_;
  Allocator& secondary_;
};

}  // namespace pw::allocator
