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

#include "pw_allocator/allocator.h"
#include "pw_metric/metric.h"

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

/// Pure virtual base class for providing allocator metrics.
///
/// Classes which implement this interface must provide a `MetricsType`:
///   - Using `internal::Metrics` will always collect metrics.
///   - Using `internal::MetricsStub` will never collect metrics.
///   - Using `internal::DefaultMetrics` will collect metrics if the
///     `pw_allocator_COLLECT_METRICS` build argument is set and enabled.
template <typename MetricsType>
class WithMetrics {
 public:
  using metrics_type = MetricsType;

  virtual ~WithMetrics() = default;

  virtual metrics_type& metric_group() = 0;
  virtual const metrics_type& metric_group() const = 0;

  uint32_t used() const { return metric_group().used(); }
  uint32_t peak() const { return metric_group().peak(); }
  uint32_t count() const { return metric_group().count(); }
};

/// Pure virtual base class that combines the allocator and metrics interfaces.
///
/// This type exists simply to make it easier to express both constraints on
/// parameters and return types.
template <typename MetricsType>
class AllocatorWithMetrics : public Allocator,
                             public WithMetrics<MetricsType> {};

}  // namespace pw::allocator
