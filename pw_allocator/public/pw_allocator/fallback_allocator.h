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
#include "pw_status/status.h"

namespace pw::allocator {

/// This class simply dispatches between a primary and secondary allocator. Any
/// attempt to allocate memory will first be handled by the primary allocator.
/// If it cannot allocate memory, e.g. because it is out of memory, the
/// secondary alloator will try to allocate memory instead.
class FallbackAllocator : public Allocator {
 public:
  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr FallbackAllocator() = default;

  /// Non-constexpr constructor that autmatically invokes `Init`.
  FallbackAllocator(Allocator& primary, Allocator& secondary)
      : FallbackAllocator() {
    Init(primary, secondary);
  }

  /// Sets the primary and secondary allocators.
  ///
  /// It is an error to call any method without calling this method first.
  void Init(Allocator& primary, Allocator& secondary) {
    primary_ = &primary;
    secondary_ = &secondary;
  }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    void* ptr = primary_->Allocate(layout);
    return ptr != nullptr ? ptr : secondary_->Allocate(layout);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override {
    if (primary_->Query(ptr, layout).ok()) {
      primary_->Deallocate(ptr, layout);
    } else {
      secondary_->Deallocate(ptr, layout);
    }
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override {
    return primary_->Query(ptr, layout).ok()
               ? primary_->Resize(ptr, layout, new_size)
               : secondary_->Resize(ptr, layout, new_size);
  }

  /// @copydoc Allocator::GetLayout
  Result<Layout> DoGetLayout(const void* ptr) const override {
    Result<Layout> primary_result = primary_->GetLayout(ptr);
    if (primary_result.ok()) {
      return primary_result;
    }
    Result<Layout> secondary_result = secondary_->GetLayout(ptr);
    if (secondary_result.ok()) {
      return secondary_result;
    }
    if (primary_result.status().IsNotFound() ||
        secondary_result.status().IsNotFound()) {
      return Status::NotFound();
    }
    return Status::Unimplemented();
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override {
    Status status = primary_->Query(ptr, layout);
    return status.ok() ? status : secondary_->Query(ptr, layout);
  }

  Allocator* primary_ = nullptr;
  Allocator* secondary_ = nullptr;
};

}  // namespace pw::allocator
