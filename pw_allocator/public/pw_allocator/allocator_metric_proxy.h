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
#include "pw_metric/metric.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// Wraps an `Allocator` and records details of its usage.
///
/// In order for this object to record memory usage metrics correctly, all calls
/// to, e.g., `Allocate`, `Deallocate`, etc. must be made through it and not the
/// allocator it wraps.
///
/// As a rule, the wrapped allocator is always invoked before any conditions are
/// asserted by this class, with the exception of checking that a wrapped
/// allocator has been set via `Initialize`. This allows the wrapped allocator
/// to issue a more detailed error in case of misuse.
class AllocatorMetricProxy : public Allocator {
 public:
  constexpr explicit AllocatorMetricProxy(metric::Token token)
      : memusage_(token) {}

  /// Sets the wrapped allocator.
  ///
  /// This must be called exactly once and before any other method.
  ///
  /// @param[in]  allocator   The allocator to wrap and record.
  void Initialize(Allocator& allocator);

  metric::Group& memusage() { return memusage_; }
  size_t used() const { return used_.value(); }
  size_t peak() const { return peak_.value(); }
  size_t count() const { return count_.value(); }

 private:
  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  metric::Group memusage_;
  Allocator* allocator_ = nullptr;
  PW_METRIC(used_, "used", 0U);
  PW_METRIC(peak_, "peak", 0U);
  PW_METRIC(count_, "count", 0U);
};

}  // namespace pw::allocator
