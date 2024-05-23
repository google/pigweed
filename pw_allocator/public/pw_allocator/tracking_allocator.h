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
#include <cstdint>
#include <cstring>

#include "pw_allocator/allocator.h"
#include "pw_allocator/capability.h"
#include "pw_allocator/metrics.h"
#include "pw_assert/assert.h"
#include "pw_metric/metric.h"
#include "pw_preprocessor/compiler.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::allocator {

/// This tag type is used to explicitly select the constructor which adds
/// the tracking allocator's metrics group as a child of the info
/// allocator it is wrapping.
static constexpr struct AddTrackingAllocatorAsChild {
} kAddTrackingAllocatorAsChild = {};

/// Wraps an `Allocator` and records details of its usage.
///
/// Metric collection is performed using the provided template parameter type.
/// Callers can not instantiate this class directly, as it lacks a public
/// constructor. Instead, callers should use derived classes which provide the
/// template parameter type, such as `TrackingAllocator` which uses the
/// default metrics implementation, or `TrackingAllocatorForTest` which
/// always uses the real metrics implementation.
template <typename MetricsType>
class TrackingAllocator : public Allocator {
 public:
  TrackingAllocator(metric::Token token, Allocator& allocator)
      : Allocator(allocator.capabilities() | kImplementsGetRequestedLayout),
        allocator_(allocator),
        metrics_(token) {}

  template <typename OtherMetrics>
  TrackingAllocator(metric::Token token,
                    TrackingAllocator<OtherMetrics>& parent,
                    const AddTrackingAllocatorAsChild&)
      : TrackingAllocator(token, parent) {
    parent.metric_group().Add(metric_group());
  }

  const metric::Group& metric_group() const { return metrics_.group(); }
  metric::Group& metric_group() { return metrics_.group(); }

  const MetricsType& metrics() const { return metrics_.metrics(); }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override { DoDeallocate(ptr); }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override;

  /// @copydoc Allocator::Reallocate
  void* DoReallocate(void* ptr, Layout new_layout) override;

  /// @copydoc Deallocator::GetInfo
  Result<Layout> DoGetInfo(InfoType info_type, const void* ptr) const override {
    return GetInfo(allocator_, info_type, ptr);
  }

  Allocator& allocator_;
  internal::Metrics<MetricsType> metrics_;
};

// Template method implementation.

template <typename MetricsType>
void* TrackingAllocator<MetricsType>::DoAllocate(Layout layout) {
  Layout requested = layout;
  void* new_ptr = allocator_.Allocate(requested);
  if (new_ptr == nullptr) {
    metrics_.RecordFailure(requested.size());
    return nullptr;
  }
  Layout allocated = Layout::Unwrap(GetAllocatedLayout(new_ptr));
  metrics_.IncrementAllocations();
  metrics_.ModifyRequested(requested.size(), 0);
  metrics_.ModifyAllocated(allocated.size(), 0);
  return new_ptr;
}

template <typename MetricsType>
void TrackingAllocator<MetricsType>::DoDeallocate(void* ptr) {
  Layout requested = Layout::Unwrap(GetRequestedLayout(ptr));
  Layout allocated = Layout::Unwrap(GetAllocatedLayout(ptr));
  allocator_.Deallocate(ptr);
  metrics_.IncrementDeallocations();
  metrics_.ModifyRequested(0, requested.size());
  metrics_.ModifyAllocated(0, allocated.size());
}

template <typename MetricsType>
bool TrackingAllocator<MetricsType>::DoResize(void* ptr, size_t new_size) {
  Layout requested = Layout::Unwrap(GetRequestedLayout(ptr));
  Layout allocated = Layout::Unwrap(GetAllocatedLayout(ptr));
  Layout new_requested(new_size, requested.alignment());
  if (!allocator_.Resize(ptr, new_requested.size())) {
    metrics_.RecordFailure(new_size);
    return false;
  }
  Layout new_allocated = Layout::Unwrap(GetAllocatedLayout(ptr));
  metrics_.IncrementResizes();
  metrics_.ModifyRequested(new_requested.size(), requested.size());
  metrics_.ModifyAllocated(new_allocated.size(), allocated.size());
  return true;
}

template <typename MetricsType>
void* TrackingAllocator<MetricsType>::DoReallocate(void* ptr,
                                                   Layout new_layout) {
  Layout requested = Layout::Unwrap(GetRequestedLayout(ptr));
  Layout allocated = Layout::Unwrap(GetAllocatedLayout(ptr));
  Layout new_requested(new_layout.size(), requested.alignment());
  void* new_ptr = allocator_.Reallocate(ptr, new_requested);
  if (new_ptr == nullptr) {
    metrics_.RecordFailure(new_requested.size());
    return nullptr;
  }
  metrics_.IncrementReallocations();
  metrics_.ModifyRequested(new_requested.size(), requested.size());
  Layout new_allocated = Layout::Unwrap(GetAllocatedLayout(new_ptr));
  if (ptr != new_ptr) {
    // Reallocate performed "alloc, copy, free". Increment and decrement
    // seperately in order to ensure "peak" metrics are correct.
    metrics_.ModifyAllocated(new_allocated.size(), 0);
    metrics_.ModifyAllocated(0, allocated.size());
  } else {
    // Reallocate performed "resize" without additional overhead.
    metrics_.ModifyAllocated(new_allocated.size(), allocated.size());
  }
  return new_ptr;
}

// TODO(b/326509341): This is an interim alias to facilitate refactoring
// downstream consumers of `TrackingAllocator` to add a template parameter.
//
// The following migration steps are complete:
// 1. Downstream consumers will be updated to use `TrackingAllocatorImpl<...>`.
// 2. The iterim `TrackingAllocator` class will be removed.
// 3. `TrackingAllocatorImpl<...>` will be renamed to `TrackingAllocator<...>`,
//    with a `TrackingAllocatorImpl<...>` alias pointing to it.
//
// The following migration steps remain:
// 4. Downstream consumers will be updated to use `TrackingAllocator<...>`.
// 5. The `TrackingAllocatorImpl<...>` alias will be removed.
template <typename MetricsType>
using TrackingAllocatorImpl = TrackingAllocator<MetricsType>;

}  // namespace pw::allocator
