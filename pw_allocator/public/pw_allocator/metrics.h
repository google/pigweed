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

#include "pw_metric/metric.h"

namespace pw::allocator {

/// Declares the names of metrics used by `pw::allocator::Metrics`.
///
/// Only the names of declared metrics may be passed to
/// ``PW_ALLOCATOR_METRICS_ENABLE`` as part of a metrics struct definition.
///
/// This macro generates trait types that are used by ``Metrics`` to
/// conditionally include metric-related code.
///
/// Note: if enabling ``peak_allocated_bytes` or `cumulative_allocated_bytes`,
/// `allocated_bytes` should also be enabled.
#define PW_ALLOCATOR_METRICS_DECLARE(metric_name)                           \
  template <typename MetricsType, typename = void>                          \
  struct has_##metric_name : std::false_type {};                            \
  template <typename MetricsType>                                           \
  struct has_##metric_name<MetricsType,                                     \
                           std::void_t<decltype(MetricsType::metric_name)>> \
      : std::true_type {}
PW_ALLOCATOR_METRICS_DECLARE(allocated_bytes);
PW_ALLOCATOR_METRICS_DECLARE(peak_allocated_bytes);
PW_ALLOCATOR_METRICS_DECLARE(cumulative_allocated_bytes);
PW_ALLOCATOR_METRICS_DECLARE(num_allocations);
PW_ALLOCATOR_METRICS_DECLARE(num_deallocations);
PW_ALLOCATOR_METRICS_DECLARE(num_resizes);
PW_ALLOCATOR_METRICS_DECLARE(num_reallocations);
PW_ALLOCATOR_METRICS_DECLARE(num_failures);
#undef PW_ALLOCATOR_METRICS_DECLARE

/// Enables a metric for in a metrics struct.
///
/// The ``pw::allocator::TrackingAllocator`` template takes a struct that
/// enables zero or more of the metrics enumerated by
/// ``PW_ALLOCATOR_METRICS_DECLARE```.
///
/// This struct may be one of ``AllMetrics`` or ``NoMetrics``, or may be a
/// custom struct that selects a subset of metrics.
///
/// Note that this must be fully-qualified since the metric struct may be
/// defined in any namespace.
///
/// Example:
/// @code{.cpp}
///   struct MyMetrics {
///     PW_ALLOCATOR_METRICS_ENABLE(allocated_bytes);
///     PW_ALLOCATOR_METRICS_ENABLE(peak_allocated_bytes);
///     PW_ALLOCATOR_METRICS_ENABLE(num_failures);
///   };
/// @endcode
#define PW_ALLOCATOR_METRICS_ENABLE(metric_name)                   \
  static_assert(!::pw::allocator::has_##metric_name<void>::value); \
  PW_METRIC(metric_name, #metric_name, 0U)

/// A predefined metric struct that enables all allocator metrics.
struct AllMetrics {
  PW_ALLOCATOR_METRICS_ENABLE(allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(peak_allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(cumulative_allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(num_allocations);
  PW_ALLOCATOR_METRICS_ENABLE(num_deallocations);
  PW_ALLOCATOR_METRICS_ENABLE(num_resizes);
  PW_ALLOCATOR_METRICS_ENABLE(num_reallocations);
  PW_ALLOCATOR_METRICS_ENABLE(num_failures);
};

/// A predefined metric struct that enables no allocator metrics.
struct NoMetrics {};

namespace internal {

/// Encapsulates the metrics struct for ``pw::allocator::TrackingAllocator``.
///
/// This class uses the type traits from ``PW_ALLOCATOR_METRICS_DECLARE`` to
/// conditionally include or exclude code to update metrics based on calls to
/// the ``pw::allocator::Allocator`` API. This minimizes code size without
/// adding additional conditions to be evaluated at runtime.
///
/// @tparam   MetricsType   The struct defining which metrics are enabled.
template <typename MetricsType>
class Metrics final {
 public:
  Metrics(metric::Token token);
  ~Metrics() = default;

  const metric::Group& group() const { return group_; }
  metric::Group& group() { return group_; }

  const MetricsType& metrics() const { return metrics_; }

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

 private:
  /// @copydoc RecordAllocation
  void RecordAllocationImpl(uint32_t new_size);

  /// @copydoc RecordDeallocation
  void RecordDeallocationImpl(uint32_t old_size);

  /// @copydoc RecordResize
  void RecordResizeImpl(uint32_t old_size, uint32_t new_size);

 private:
  metric::Group group_;
  MetricsType metrics_;
};

// Helper method for converting `size_t`s to `uint32_t`s.
inline uint32_t ClampU32(size_t size) {
  return static_cast<uint32_t>(std::min(
      size, static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
}

// Template method implementation.

template <typename MetricsType>
Metrics<MetricsType>::Metrics(metric::Token token) : group_(token) {
  if constexpr (has_allocated_bytes<MetricsType>::value) {
    group_.Add(metrics_.allocated_bytes);
  }
  if constexpr (has_peak_allocated_bytes<MetricsType>::value) {
    group_.Add(metrics_.peak_allocated_bytes);
  }
  if constexpr (has_cumulative_allocated_bytes<MetricsType>::value) {
    group_.Add(metrics_.cumulative_allocated_bytes);
  }
  if constexpr (has_num_allocations<MetricsType>::value) {
    group_.Add(metrics_.num_allocations);
  }
  if constexpr (has_num_deallocations<MetricsType>::value) {
    group_.Add(metrics_.num_deallocations);
  }
  if constexpr (has_num_resizes<MetricsType>::value) {
    group_.Add(metrics_.num_resizes);
  }
  if constexpr (has_num_reallocations<MetricsType>::value) {
    group_.Add(metrics_.num_reallocations);
  }
  if constexpr (has_num_failures<MetricsType>::value) {
    group_.Add(metrics_.num_failures);
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordAllocation(size_t new_size) {
  RecordAllocationImpl(internal::ClampU32(new_size));
  if constexpr (has_num_allocations<MetricsType>::value) {
    metrics_.num_allocations.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordAllocationImpl(uint32_t new_size) {
  if constexpr (has_allocated_bytes<MetricsType>::value) {
    metrics_.allocated_bytes.Increment(new_size);
    if constexpr (has_peak_allocated_bytes<MetricsType>::value) {
      uint32_t allocated_bytes = metrics_.allocated_bytes.value();
      if (metrics_.peak_allocated_bytes.value() < allocated_bytes) {
        metrics_.peak_allocated_bytes.Set(allocated_bytes);
      }
    }
    if constexpr (has_cumulative_allocated_bytes<MetricsType>::value) {
      metrics_.cumulative_allocated_bytes.Increment(new_size);
    }
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordDeallocation(size_t old_size) {
  RecordDeallocationImpl(internal::ClampU32(old_size));
  if constexpr (has_num_deallocations<MetricsType>::value) {
    metrics_.num_deallocations.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordDeallocationImpl(uint32_t old_size) {
  if constexpr (has_allocated_bytes<MetricsType>::value) {
    metrics_.allocated_bytes.Decrement(old_size);
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordResize(size_t old_size, size_t new_size) {
  RecordResizeImpl(internal::ClampU32(old_size), internal::ClampU32(new_size));
  if constexpr (has_num_resizes<MetricsType>::value) {
    metrics_.num_resizes.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordResizeImpl(uint32_t old_size,
                                            uint32_t new_size) {
  if constexpr (has_allocated_bytes<MetricsType>::value) {
    metrics_.allocated_bytes.Decrement(old_size);
    metrics_.allocated_bytes.Increment(new_size);
    if (old_size < new_size) {
      if constexpr (has_peak_allocated_bytes<MetricsType>::value) {
        uint32_t allocated_bytes = metrics_.allocated_bytes.value();
        if (metrics_.peak_allocated_bytes.value() < allocated_bytes) {
          metrics_.peak_allocated_bytes.Set(allocated_bytes);
        }
      }
      if constexpr (has_cumulative_allocated_bytes<MetricsType>::value) {
        metrics_.cumulative_allocated_bytes.Increment(new_size - old_size);
      }
    }
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordReallocation(size_t old_size,
                                              size_t new_size,
                                              bool moved) {
  if (moved) {
    RecordAllocationImpl(internal::ClampU32(new_size));
    RecordDeallocationImpl(internal::ClampU32(old_size));
  } else {
    RecordResizeImpl(internal::ClampU32(old_size),
                     internal::ClampU32(new_size));
  }
  if constexpr (has_num_reallocations<MetricsType>::value) {
    metrics_.num_reallocations.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordFailure() {
  if constexpr (has_num_failures<MetricsType>::value) {
    metrics_.num_failures.Increment();
  }
}

}  // namespace internal
}  // namespace pw::allocator
