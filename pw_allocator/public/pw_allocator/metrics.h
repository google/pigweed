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
  void Init(size_t capacity);

  /// Records the details of allocating memory and updates metrics accordingly.
  ///
  /// @param  new_size          Size of the allocated memory.
  void RecordAllocation(size_t new_size);

  /// Records the details of deallocating memory and updates metrics
  /// accordingly.
  ///
  /// @param  old_size          Size of the previously allocated memory.
  void RecordDeallocation(size_t old_size);

  /// Records the details of resizing an allocation and updates metrics
  /// accordingly.
  ///
  /// @param  old_size          Previous size of the allocated memory.
  /// @param  new_size          Size of the allocated memory.
  void RecordResize(size_t old_size, size_t new_size);

  /// Records the details of reallocating memory and updates metrics
  /// accordingly.
  ///
  /// @param  old_size          Previous size of the allocated memory.
  /// @param  new_size          Size of the allocated memory.
  /// @param  moved             True if the memory was copied to a new location.
  void RecordReallocation(size_t old_size, size_t new_size, bool moved);

  /// Records that a call to `Allocate`, `Resize`, or `Reallocate` failed. This
  /// may indicated memory becoming exhausted and/or high fragmentation.
  void RecordFailure();

  uint32_t total_bytes() const { return total_bytes_.value(); }
  uint32_t allocated_bytes() const { return allocated_bytes_.value(); }
  uint32_t peak_allocated_bytes() const {
    return peak_allocated_bytes_.value();
  }
  uint32_t cumulative_allocated_bytes() const {
    return cumulative_allocated_bytes_.value();
  }
  uint32_t num_allocations() const { return num_allocations_.value(); }
  uint32_t num_deallocations() const { return num_deallocations_.value(); }
  uint32_t num_resizes() const { return num_resizes_.value(); }
  uint32_t num_reallocations() const { return num_reallocations_.value(); }
  uint32_t num_failures() const { return num_failures_.value(); }

 private:
  /// @copydoc RecordAllocation
  void RecordAllocationImpl(uint32_t new_size);

  /// @copydoc RecordDeallocation
  void RecordDeallocationImpl(uint32_t old_size);

  /// @copydoc RecordResize
  void RecordResizeImpl(uint32_t old_size, uint32_t new_size);

  PW_METRIC(total_bytes_, "total_bytes", 0U);
  PW_METRIC(allocated_bytes_, "allocated_bytes", 0U);
  PW_METRIC(peak_allocated_bytes_, "peak_allocated_bytes", 0U);
  PW_METRIC(cumulative_allocated_bytes_, "cumulative_allocated_bytes", 0U);
  PW_METRIC(num_allocations_, "num_allocations", 0U);
  PW_METRIC(num_deallocations_, "num_deallocations", 0U);
  PW_METRIC(num_resizes_, "num_resizes", 0U);
  PW_METRIC(num_reallocations_, "num_reallocations", 0U);
  PW_METRIC(num_failures_, "num_failures", 0U);
};

/// Stub implementation of the `Metrics` class above.
///
/// This class provides the same interface as `Metrics`, but each methods has no
/// effect.
class MetricsStub {
 public:
  constexpr MetricsStub(metric::Token) {}

  /// @copydoc `Metrics::Init`.
  void Init(size_t) {}

  /// Like `pw::metric::Group::Add`, but for this stub object.
  void Add(MetricsStub&) {}

  /// @copydoc `Metrics::RecordAllocation`.
  void RecordAllocation(size_t) {}

  /// @copydoc `Metrics::RecordDeallocation`.
  void RecordDeallocation(size_t) {}

  /// @copydoc `Metrics::RecordResize`.
  void RecordResize(size_t, size_t) {}

  /// @copydoc `Metrics::RecordReallocation`.
  void RecordReallocation(size_t, size_t, bool) {}

  /// @copydoc `Metrics::RecordFailure`.
  void RecordFailure() {}

  uint32_t total_bytes() const { return 0; }
  uint32_t allocated_bytes() const { return 0; }
  uint32_t peak_allocated_bytes() const { return 0; }
  uint32_t cumulative_allocated_bytes() const { return 0; }
  uint32_t num_allocations() const { return 0; }
  uint32_t num_deallocations() const { return 0; }
  uint32_t num_resizes() const { return 0; }
  uint32_t num_reallocations() const { return 0; }
  uint32_t num_failures() const { return 0; }

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

  uint32_t total_bytes() const { return metric_group().total_bytes(); }
  uint32_t allocated_bytes() const { return metric_group().allocated_bytes(); }
  uint32_t peak_allocated_bytes() const {
    return metric_group().peak_allocated_bytes();
  }
  uint32_t cumulative_allocated_bytes() const {
    return metric_group().cumulative_allocated_bytes();
  }
  uint32_t num_allocations() const { return metric_group().num_allocations(); }
  uint32_t num_deallocations() const {
    return metric_group().num_deallocations();
  }
  uint32_t num_resizes() const { return metric_group().num_resizes(); }
  uint32_t num_reallocations() const {
    return metric_group().num_reallocations();
  }
  uint32_t num_failures() const { return metric_group().num_failures(); }

 protected:
  /// @copydoc `Metrics::RecordAllocation`.
  void RecordAllocation(size_t new_size) {
    metric_group().RecordAllocation(new_size);
  }

  /// @copydoc `Metrics::RecordDeallocation`.
  void RecordDeallocation(size_t old_size) {
    metric_group().RecordDeallocation(old_size);
  }

  /// @copydoc `Metrics::RecordResize`.
  void RecordResize(size_t old_size, size_t new_size) {
    metric_group().RecordResize(old_size, new_size);
  }

  /// @copydoc `Metrics::RecordReallocation`.
  void RecordReallocation(size_t old_size, size_t new_size, bool moved) {
    metric_group().RecordReallocation(old_size, new_size, moved);
  }

  /// @copydoc `Metrics::RecordFailure`.
  void RecordFailure() { metric_group().RecordFailure(); }
};

/// Pure virtual base class that combines the allocator and metrics interfaces.
///
/// This type exists simply to make it easier to express both constraints on
/// parameters and return types.
template <typename MetricsType>
class AllocatorWithMetrics : public Allocator,
                             public WithMetrics<MetricsType> {};

}  // namespace pw::allocator
