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
#include "pw_allocator/metrics.h"
#include "pw_metric/metric.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// This tag type is used to explicitly select the constructor which adds
/// the tracking allocator's metrics group as a child of the tracking
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
class TrackingAllocatorImpl : public Allocator {
 public:
  using metric_type = MetricsType;

  TrackingAllocatorImpl(metric::Token token, Allocator& allocator)
      : Allocator(allocator.capabilities()),
        allocator_(allocator),
        metrics_(token) {}

  template <typename OtherMetrics>
  TrackingAllocatorImpl(metric::Token token,
                        TrackingAllocatorImpl<OtherMetrics>& parent,
                        const AddTrackingAllocatorAsChild&)
      : TrackingAllocatorImpl(token, parent) {
    parent.metric_group().Add(metric_group());
  }

  const metric::Group& metric_group() const { return metrics_.group(); }
  metric::Group& metric_group() { return metrics_.group(); }

  const MetricsType& metrics() const { return metrics_.metrics(); }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    void* ptr = allocator_.Allocate(layout);
    if (ptr == nullptr) {
      metrics_.RecordFailure();
      return nullptr;
    }
    metrics_.RecordAllocation(layout.size());
    return ptr;
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override {
    allocator_.Deallocate(ptr, layout);
    metrics_.RecordDeallocation(layout.size());
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override {
    if (!allocator_.Resize(ptr, layout, new_size)) {
      metrics_.RecordFailure();
      return false;
    }
    metrics_.RecordResize(layout.size(), new_size);
    return true;
  }

  /// @copydoc Allocator::Reallocate
  void* DoReallocate(void* ptr, Layout layout, size_t new_size) override {
    void* new_ptr = allocator_.Reallocate(ptr, layout, new_size);
    if (new_ptr == nullptr) {
      metrics_.RecordFailure();
      return nullptr;
    }
    metrics_.RecordReallocation(layout.size(), new_size, new_ptr != ptr);
    return new_ptr;
  }

  /// @copydoc Allocator::GetLayout
  Result<Layout> DoGetLayout(const void* ptr) const override {
    return allocator_.GetLayout(ptr);
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override {
    return allocator_.Query(ptr, layout);
  }

  Allocator& allocator_;
  internal::Metrics<MetricsType> metrics_;
};

// TODO(b/326509341): This is an interim class to facilitate refactoring
// downstream consumers of `TrackingAllocator` to add a template parameter.
//
// The migration will be performed as follows:
// 1. Downstream consumers will be updated to use `TrackingAllocatorImpl<...>`.
// 2. The iterim `TrackingAllocator` class will be removed.
// 3. `TrackingAllocatorImpl<...>` will be renamed to `TrackingAllocator<...>`,
//    with a `TrackingAllocatorImpl<...>` alias pointing to it.
// 4. Downstream consumers will be updated to use `TrackingAllocator<...>`.
// 5. The `TrackingAllocatorImpl<...>` alias will be removed.
class TrackingAllocator : public TrackingAllocatorImpl<AllMetrics> {
 public:
  TrackingAllocator(metric::Token token, Allocator& allocator)
      : TrackingAllocatorImpl<AllMetrics>(token, allocator) {}
  uint32_t allocated_bytes() const { return metrics().allocated_bytes.value(); }
  uint32_t peak_allocated_bytes() const {
    return metrics().peak_allocated_bytes.value();
  }
  uint32_t cumulative_allocated_bytes() const {
    return metrics().cumulative_allocated_bytes.value();
  }
  uint32_t num_allocations() const { return metrics().num_allocations.value(); }
  uint32_t num_deallocations() const {
    return metrics().num_deallocations.value();
  }
  uint32_t num_resizes() const { return metrics().num_resizes.value(); }
  uint32_t num_reallocations() const {
    return metrics().num_reallocations.value();
  }
  uint32_t num_failures() const { return metrics().num_failures.value(); }
};

}  // namespace pw::allocator
