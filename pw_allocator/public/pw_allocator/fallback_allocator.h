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
#include "pw_allocator/layout.h"
#include "pw_result/result.h"
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
  FallbackAllocator(Allocator& primary, Allocator& secondary);

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override;

  /// @copydoc Deallocator::GetInfo
  Result<Layout> DoGetInfo(InfoType info_type, const void* ptr) const override;

  Allocator& primary_;
  Allocator& secondary_;
};

}  // namespace pw::allocator
