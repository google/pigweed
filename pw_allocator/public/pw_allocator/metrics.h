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

// Tracks the current, peak, and cumulative number of bytes requested to be
// allocated, respectively.
PW_ALLOCATOR_METRICS_DECLARE(requested_bytes);
PW_ALLOCATOR_METRICS_DECLARE(peak_requested_bytes);
PW_ALLOCATOR_METRICS_DECLARE(cumulative_requested_bytes);

// Tracks the current, peak, and cumulative number of bytes actually allocated,
// respectively.
PW_ALLOCATOR_METRICS_DECLARE(allocated_bytes);
PW_ALLOCATOR_METRICS_DECLARE(peak_allocated_bytes);
PW_ALLOCATOR_METRICS_DECLARE(cumulative_allocated_bytes);

// Tracks the number of successful calls to each interface method.
PW_ALLOCATOR_METRICS_DECLARE(num_allocations);
PW_ALLOCATOR_METRICS_DECLARE(num_deallocations);
PW_ALLOCATOR_METRICS_DECLARE(num_resizes);
PW_ALLOCATOR_METRICS_DECLARE(num_reallocations);

// Tracks the number of interface calls that failed, and the number of bytes
// requested in those calls.
PW_ALLOCATOR_METRICS_DECLARE(num_failures);
PW_ALLOCATOR_METRICS_DECLARE(unfulfilled_bytes);

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
  PW_ALLOCATOR_METRICS_ENABLE(requested_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(peak_requested_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(cumulative_requested_bytes);

  PW_ALLOCATOR_METRICS_ENABLE(allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(peak_allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(cumulative_allocated_bytes);

  PW_ALLOCATOR_METRICS_ENABLE(num_allocations);
  PW_ALLOCATOR_METRICS_ENABLE(num_deallocations);
  PW_ALLOCATOR_METRICS_ENABLE(num_resizes);
  PW_ALLOCATOR_METRICS_ENABLE(num_reallocations);

  PW_ALLOCATOR_METRICS_ENABLE(num_failures);
  PW_ALLOCATOR_METRICS_ENABLE(unfulfilled_bytes);
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

  /// Updates how much memory was requested and successfully allocated.
  ///
  /// This will update the current, peak, and cumulative amounts of memory
  /// requests that were satisfied by an allocator.
  ///
  /// @param  increase         How much memory was requested to be allocated.
  /// @param  decrease         How much memory was requested to be freed.
  void ModifyRequested(size_t increase, size_t decrease);

  /// Updates how much memory is allocated.
  ///
  /// This will update the current, peak, and cumulative amounts of memory that
  /// has been actually allocated or freed. This method acts as if it frees
  /// memory before allocating. If a routine suchas `Reallocate` allocates
  /// before freeing, the update should be separated into two calls, e.g.
  ///
  /// @code{.cpp}
  /// ModifyAllocated(increase, 0);
  /// ModifyAllocated(0, decrease);
  /// @endcode
  ///
  /// @param  increase         How much memory was allocated.
  /// @param  decrease         How much memory was freed.
  void ModifyAllocated(size_t increase, size_t decrease);

  /// Records that a call to `Allocate` was made.
  void IncrementAllocations();

  /// Records that a call to `Deallocate` was made.
  void IncrementDeallocations();

  /// Records that a call to `Resize` was made.
  void IncrementResizes();

  /// Records that a call to `Reallocate` was made.
  void IncrementReallocations();

  /// Records that a call to `Allocate`, `Resize`, or `Reallocate` failed.
  ///
  /// This may indicated that memory becoming exhausted and/or highly
  /// fragmented.
  ///
  /// @param  requested         How much memory was requested in the failed
  ///                           call.
  void RecordFailure(size_t requested);

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
  if constexpr (has_requested_bytes<MetricsType>::value) {
    group_.Add(metrics_.requested_bytes);
  }
  if constexpr (has_peak_requested_bytes<MetricsType>::value) {
    group_.Add(metrics_.peak_requested_bytes);
  }
  if constexpr (has_cumulative_requested_bytes<MetricsType>::value) {
    group_.Add(metrics_.cumulative_requested_bytes);
  }
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
  if constexpr (has_unfulfilled_bytes<MetricsType>::value) {
    group_.Add(metrics_.unfulfilled_bytes);
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::ModifyRequested(size_t increase, size_t decrease) {
  if constexpr (has_requested_bytes<MetricsType>::value) {
    metrics_.requested_bytes.Increment(internal::ClampU32(increase));
    metrics_.requested_bytes.Decrement(internal::ClampU32(decrease));
    if constexpr (has_peak_requested_bytes<MetricsType>::value) {
      uint32_t requested_bytes = metrics_.requested_bytes.value();
      if (metrics_.peak_requested_bytes.value() < requested_bytes) {
        metrics_.peak_requested_bytes.Set(requested_bytes);
      }
    }
    if constexpr (has_cumulative_requested_bytes<MetricsType>::value) {
      if (increase > decrease) {
        metrics_.cumulative_requested_bytes.Increment(
            internal::ClampU32(increase - decrease));
      }
    }
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::ModifyAllocated(size_t increase, size_t decrease) {
  if constexpr (has_allocated_bytes<MetricsType>::value) {
    metrics_.allocated_bytes.Increment(internal::ClampU32(increase));
    metrics_.allocated_bytes.Decrement(internal::ClampU32(decrease));
    if constexpr (has_peak_allocated_bytes<MetricsType>::value) {
      uint32_t allocated_bytes = metrics_.allocated_bytes.value();
      if (metrics_.peak_allocated_bytes.value() < allocated_bytes) {
        metrics_.peak_allocated_bytes.Set(allocated_bytes);
      }
    }
    if constexpr (has_cumulative_allocated_bytes<MetricsType>::value) {
      if (increase > decrease) {
        metrics_.cumulative_allocated_bytes.Increment(
            internal::ClampU32(increase - decrease));
      }
    }
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::IncrementAllocations() {
  if constexpr (has_num_allocations<MetricsType>::value) {
    metrics_.num_allocations.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::IncrementDeallocations() {
  if constexpr (has_num_deallocations<MetricsType>::value) {
    metrics_.num_deallocations.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::IncrementResizes() {
  if constexpr (has_num_resizes<MetricsType>::value) {
    metrics_.num_resizes.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::IncrementReallocations() {
  if constexpr (has_num_reallocations<MetricsType>::value) {
    metrics_.num_reallocations.Increment();
  }
}

template <typename MetricsType>
void Metrics<MetricsType>::RecordFailure(size_t requested) {
  if constexpr (has_num_failures<MetricsType>::value) {
    metrics_.num_failures.Increment();
  }
  if constexpr (has_unfulfilled_bytes<MetricsType>::value) {
    metrics_.unfulfilled_bytes.Increment(internal::ClampU32(requested));
  }
}

}  // namespace internal
}  // namespace pw::allocator
