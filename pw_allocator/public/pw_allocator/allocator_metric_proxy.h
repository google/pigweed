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
#include "pw_status/status.h"

namespace pw::allocator {

/// Wraps an `Allocator` and records details of its usage.
///
/// Metric collection is performed using the provided template parameter type.
/// Callers can not instantiate this class directly, as it lacks a public
/// constructor. Instead, callers should use derived classes which provide the
/// template parameter type, such as `AllocatorMetricProxy` which uses the
/// default metrics implementation, or `AllocatorMetricProxyForTest` which
/// always uses the real metrics implementation.
template <typename MetricsType>
class AllocatorMetricProxyImpl : public AllocatorWithMetrics<MetricsType> {
 public:
  using metrics_type = MetricsType;

  constexpr explicit AllocatorMetricProxyImpl(metric::Token token)
      : allocator_(nullptr), metrics_(token) {}

  /// Sets the wrapped allocator.
  ///
  /// This must be called exactly once and before any other method.
  ///
  /// @param[in]  allocator   The allocator to wrap and record.
  void Init(Allocator& allocator) {
    allocator_ = &allocator;
    metrics_.Init();
  }

  /// @copydoc `Init`.
  void Initialize(Allocator& allocator) { Init(allocator); }

  metrics_type& metric_group() override { return metrics_; }
  const metrics_type& metric_group() const override { return metrics_; }

 private:
  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override {
    return allocator_->Query(ptr, layout);
  }

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    void* ptr = allocator_->Allocate(layout);
    if (ptr == nullptr) {
      return nullptr;
    }
    metrics_.Update(0, layout.size());
    return ptr;
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override {
    if (ptr != nullptr) {
      allocator_->Deallocate(ptr, layout);
      metrics_.Update(layout.size(), 0);
    }
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override {
    if (!allocator_->Resize(ptr, layout, new_size)) {
      return false;
    }
    metrics_.Update(layout.size(), new_size);
    return true;
  }

  Allocator* allocator_;
  metrics_type metrics_;
};

/// Allocator metric proxy that uses the default metrics implementation.
///
/// Depending on the value of the `pw_allocator_COLLECT_METRICS` build argument,
/// the `internal::DefaultMetrics` type is an alias for either the real or stub
/// metrics implementation.
using AllocatorMetricProxy = AllocatorMetricProxyImpl<internal::DefaultMetrics>;

}  // namespace pw::allocator
