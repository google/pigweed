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
namespace internal {

/// Allocation metrics.
///
/// This class encapsulates metric collection by the proxy in order to
/// reduce the number of preprocessor statements for conditional compilation.
class Metrics : public metric::Group {
 public:
  constexpr Metrics(metric::Token token) : metric::Group(token) {}

  /// Add the metrics to the group.
  ///
  /// A separate `Init` method allows the constructor to remain constexpr.
  void Init();

  /// Updates the allocation statistics.
  ///
  /// An `old_size` of 0 represents an allocation.
  /// A `new_size` of 0 represents a deallocation.
  void Update(size_t old_size, size_t new_size);

  uint32_t used() const { return used_.value(); }
  uint32_t peak() const { return peak_.value(); }
  uint32_t count() const { return count_.value(); }

 private:
  PW_METRIC(used_, "used", 0U);
  PW_METRIC(peak_, "peak", 0U);
  PW_METRIC(count_, "count", 0U);
};

/// Stub implementation of the `Metrics` class above.
///
/// This class provides the same interface as `Metrics`, but each methods has no
/// effect.
class MetricsStub {
 public:
  constexpr MetricsStub(metric::Token) {}

  /// @copydoc `Metrics::Init`.
  void Init() {}

  /// Like `pw::metric::Group::Add`, but for this stub object.
  void Add(MetricsStub&) {}

  /// @copydoc `Metrics::Update`.
  void Update(size_t, size_t) {}

  uint32_t used() const { return 0; }
  uint32_t peak() const { return 0; }
  uint32_t count() const { return 0; }

  /// @copydoc pw::metric::Group::Dump.
  void Dump() {}
  void Dump(int) {}
};

/// If the `pw_allocator_COLLECT_METRICS` build argument is enabled,
/// `AllocatorMetricProxy` will use `Metrics` by default. If it is disabled or
/// not set, it will use the `MetricsStub`.
#if defined(PW_ALLOCATOR_COLLECT_METRICS) && PW_ALLOCATOR_COLLECT_METRICS
using DefaultMetrics = Metrics;
#else
using DefaultMetrics = MetricsStub;
#endif

}  // namespace internal

/// Wraps an `Allocator` and records details of its usage.
///
/// Metric collection is performed using the provided template parameter type.
/// Callers can not instantiate this class directly, as it lacks a public
/// constructor. Instead, callers should use derived classes which provide the
/// template parameter type, such as `AllocatorMetricProxy` which uses the
/// default metrics implementation, or `AllocatorMetricProxyForTest` which
/// always uses the real metrics implementation.
template <typename M>
class AllocatorMetricProxyImpl : public Allocator {
 public:
  using MetricsType = M;

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

  MetricsType& metric_group() { return metrics_; }
  const MetricsType& metric_group() const { return metrics_; }

  uint32_t used() const { return metrics_.used(); }
  uint32_t peak() const { return metrics_.peak(); }
  uint32_t count() const { return metrics_.count(); }

 protected:
  constexpr explicit AllocatorMetricProxyImpl(metric::Token token)
      : allocator_(nullptr), metrics_(token) {}

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
  MetricsType metrics_;
};

/// Allocator metric proxy that uses the default metrics implementation.
///
/// Depending on the value of the `pw_allocator_COLLECT_METRICS` build argument,
/// the `internal::DefaultMetrics` type is an alias for either the real or stub
/// metrics implementation.
class AllocatorMetricProxy
    : public AllocatorMetricProxyImpl<internal::DefaultMetrics> {
 public:
  constexpr explicit AllocatorMetricProxy(metric::Token token)
      : AllocatorMetricProxyImpl(token) {}
};

}  // namespace pw::allocator
