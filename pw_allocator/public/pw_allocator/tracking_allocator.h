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

/// Wraps an `Allocator` and records details of its usage.
///
/// Metric collection is performed using the provided template parameter type.
/// Callers can not instantiate this class directly, as it lacks a public
/// constructor. Instead, callers should use derived classes which provide the
/// template parameter type, such as `TrackingAllocator` which uses the
/// default metrics implementation, or `TrackingAllocatorForTest` which
/// always uses the real metrics implementation.
template <typename MetricsType>
class TrackingAllocatorImpl : public AllocatorWithMetrics<MetricsType> {
 public:
  using metrics_type = MetricsType;
  using Base = AllocatorWithMetrics<MetricsType>;

  /// Constexpr constructor. Callers must explicitly call `Init`.
  constexpr explicit TrackingAllocatorImpl(metric::Token token)
      : metrics_(token) {}

  /// Non-constexpr constructor that autmatically invokes `Init`.
  TrackingAllocatorImpl(metric::Token token,
                        Allocator& allocator,
                        size_t capacity)
      : TrackingAllocatorImpl(token) {
    Init(allocator, capacity);
  }

  metrics_type& metric_group() override { return metrics_; }
  const metrics_type& metric_group() const override { return metrics_; }

  /// Sets the wrapped allocator.
  ///
  /// All other methods will fail until this method is called.
  ///
  /// @param[in]  allocator   The allocator to wrap and record.
  void Init(Allocator& allocator, size_t capacity) {
    allocator_ = &allocator;
    metrics_.Init(capacity);
  }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    if (allocator_ == nullptr) {
      return nullptr;
    }
    void* ptr = allocator_->Allocate(layout);
    if (ptr == nullptr) {
      metrics_.RecordFailure();
      return nullptr;
    }
    metrics_.RecordAllocation(layout.size());
    return ptr;
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override {
    if (allocator_ != nullptr && ptr != nullptr) {
      allocator_->Deallocate(ptr, layout);
      metrics_.RecordDeallocation(layout.size());
    }
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override {
    if (allocator_ == nullptr) {
      return false;
    }
    if (!allocator_->Resize(ptr, layout, new_size)) {
      metrics_.RecordFailure();
      return false;
    }
    metrics_.RecordResize(layout.size(), new_size);
    return true;
  }
  /// @copydoc Allocator::GetLayout
  Result<Layout> DoGetLayout(const void* ptr) const override {
    return allocator_->GetLayout(ptr);
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override {
    return allocator_->Query(ptr, layout);
  }

  /// @copydoc Allocator::Reallocate
  void* DoReallocate(void* ptr, Layout layout, size_t new_size) override {
    if (allocator_ == nullptr) {
      return nullptr;
    }
    void* new_ptr = allocator_->Reallocate(ptr, layout, new_size);
    if (new_ptr == nullptr) {
      metrics_.RecordFailure();
      return nullptr;
    }
    metrics_.RecordReallocation(layout.size(), new_size, new_ptr != ptr);
    return new_ptr;
  }

  Allocator* allocator_ = nullptr;
  metrics_type metrics_;
};

/// Allocator metric proxy that uses the default metrics implementation.
///
/// Depending on the value of the `pw_allocator_COLLECT_METRICS` build argument,
/// the `internal::DefaultMetrics` type is an alias for either the real or stub
/// metrics implementation.
using TrackingAllocator = TrackingAllocatorImpl<internal::DefaultMetrics>;

}  // namespace pw::allocator
