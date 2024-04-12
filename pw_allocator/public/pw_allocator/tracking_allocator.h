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
///
/// If the `requested_bytes` metric is enabled, then this allocator will add
/// overhead to each allocation. It uses one of two layouts:
///
///   * If the underlying allocator implements `GetAllocated`, this allocator
///     will store the requested size of an allocation after that allocation's
///     usable space. For example, assume `Layout{.size=256, .alignment=16}`,
///     `sizeof(size_t) == 4`, and that 'U' indicates "usable space". An
///     allocation might look like:
///
///     ..1f0 | UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU
///     ..... | ...
///     ..2e0 | UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU
///     ..2f0 | 00 00 01 00 .. .. .. .. .. .. .. .. .. .. .. .. // size suffix
///
///   * If the underlying allocator does NOT implement `GetAllocated`, this
///     allocator will store a `Layout` of the requested bytes and alignment
///     before the usable space. This is more expensive, as `alignment` bytes
///     must be added to keep the usable space aligned. For example, with the
///     same assumptions as beforea and 'x' indicating padding, an allocation
///     might look like:
///
///     ..1f0 | xx xx xx xx xx xx xx xx 00 00 01 00 00 00 00 10
///     ..2e0 | UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU
///     ..... | ...
///     ..2f0 | UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU UU
///
/// If the `requested_bytes` metric is disabled, no additional overhead will be
/// added.
template <typename MetricsType>
class TrackingAllocator : public Allocator {
 public:
  using metric_type = MetricsType;

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

  /// @copydoc Allocator::GetCapacity
  StatusWithSize DoGetCapacity() const override {
    return allocator_.GetCapacity();
  }

  /// @copydoc Allocator::GetRequestedLayout
  Result<Layout> DoGetRequestedLayout(const void* ptr) const override {
    return DoGetRequestedLayoutImpl(FromUsableSpace(ptr));
  }

  /// Like ``DoGetRequestedLayout``, except that ``ptr`` must be a pointer
  /// returned by the underlying allocator instead of by this object.
  Result<Layout> DoGetRequestedLayoutImpl(const void* ptr) const;

  /// @copydoc Allocator::GetUsableLayout
  Result<Layout> DoGetUsableLayout(const void* ptr) const override;

  /// @copydoc Allocator::GetAllocatedLayout
  Result<Layout> DoGetAllocatedLayout(const void* ptr) const override;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr) const override {
    return Query(allocator_, ptr);
  }

  /// Returns a layout that includes storage for an allocation and its stored
  /// details.
  Layout ReserveStorage(Layout requested);

  /// Returns a pointer to an allocation from a pointer to its usable space.
  template <typename PtrType = void*>
  PtrType FromUsableSpace(PtrType ptr) const;

  /// Returns a pointer to usable space from a pointer to an allocation.
  void* ToUsableSpace(void* ptr, const Layout& requested) const;

  /// Stores the requested size of an allocation in that allocation.
  void SetRequested(void* ptr, const Layout& requested);

  /// Returns a pointer to where the requested allocation size is stored.
  void* GetRequestedStorage(const void* ptr) const;

  Allocator& allocator_;
  internal::Metrics<MetricsType> metrics_;
};

// Template method implementation.

namespace internal {

/// Extracts a ``Layout`` from a ``Result`` if ok, or provides a default
/// ``Layout`` otherwise.
inline Layout UnwrapLayout(const Result<Layout>& result) {
  return result.ok() ? result.value() : Layout(0, 1);
}

}  // namespace internal

template <typename MetricsType>
void* TrackingAllocator<MetricsType>::DoAllocate(Layout layout) {
  Layout requested = layout;
  void* new_ptr = allocator_.Allocate(ReserveStorage(requested));
  if (new_ptr == nullptr) {
    metrics_.RecordFailure(requested.size());
    return nullptr;
  }
  Layout allocated =
      internal::UnwrapLayout(GetAllocatedLayout(allocator_, new_ptr));
  metrics_.IncrementAllocations();
  metrics_.ModifyRequested(requested.size(), 0);
  metrics_.ModifyAllocated(allocated.size(), 0);
  SetRequested(new_ptr, requested);
  return ToUsableSpace(new_ptr, requested);
}

template <typename MetricsType>
void TrackingAllocator<MetricsType>::DoDeallocate(void* ptr) {
  ptr = FromUsableSpace(ptr);
  Layout requested = internal::UnwrapLayout(DoGetRequestedLayoutImpl(ptr));
  Layout allocated =
      internal::UnwrapLayout(GetAllocatedLayout(allocator_, ptr));
  allocator_.Deallocate(ptr);
  metrics_.IncrementDeallocations();
  metrics_.ModifyRequested(0, requested.size());
  metrics_.ModifyAllocated(0, allocated.size());
}

template <typename MetricsType>
bool TrackingAllocator<MetricsType>::DoResize(void* ptr, size_t new_size) {
  ptr = FromUsableSpace(ptr);
  Layout requested = internal::UnwrapLayout(DoGetRequestedLayoutImpl(ptr));
  Layout allocated =
      internal::UnwrapLayout(GetAllocatedLayout(allocator_, ptr));
  Layout new_requested(new_size, requested.alignment());
  if (!allocator_.Resize(ptr, ReserveStorage(new_requested).size())) {
    metrics_.RecordFailure(new_size);
    return false;
  }
  Layout new_allocated =
      internal::UnwrapLayout(GetAllocatedLayout(allocator_, ptr));
  metrics_.IncrementResizes();
  metrics_.ModifyRequested(new_requested.size(), requested.size());
  metrics_.ModifyAllocated(new_allocated.size(), allocated.size());
  SetRequested(ptr, new_requested);
  return ToUsableSpace(ptr, new_requested);
}

template <typename MetricsType>
void* TrackingAllocator<MetricsType>::DoReallocate(void* ptr,
                                                   Layout new_layout) {
  ptr = FromUsableSpace(ptr);
  Layout requested = internal::UnwrapLayout(DoGetRequestedLayoutImpl(ptr));
  Layout allocated =
      internal::UnwrapLayout(GetAllocatedLayout(allocator_, ptr));
  Layout new_requested(new_layout.size(), requested.alignment());
  void* new_ptr = allocator_.Reallocate(ptr, ReserveStorage(new_requested));
  if (new_ptr == nullptr) {
    metrics_.RecordFailure(new_requested.size());
    return nullptr;
  }
  metrics_.IncrementReallocations();
  metrics_.ModifyRequested(new_requested.size(), requested.size());
  Layout new_allocated =
      internal::UnwrapLayout(GetAllocatedLayout(allocator_, new_ptr));
  if (ptr != new_ptr) {
    // Reallocate performed "alloc, copy, free". Increment and decrement
    // seperately in order to ensure "peak" metrics are correct.
    metrics_.ModifyAllocated(new_allocated.size(), 0);
    metrics_.ModifyAllocated(0, allocated.size());
  } else {
    // Reallocate performed "resize" without additional overhead.
    metrics_.ModifyAllocated(new_allocated.size(), allocated.size());
  }
  SetRequested(new_ptr, new_requested);
  return ToUsableSpace(new_ptr, new_requested);
}

template <typename MetricsType>
Result<Layout> TrackingAllocator<MetricsType>::DoGetRequestedLayoutImpl(
    const void* ptr) const {
  Result<Layout> requested = GetRequestedLayout(allocator_, ptr);
  if (requested.ok()) {
    return requested;
  }
  if constexpr (has_requested_bytes<MetricsType>::value) {
    Layout layout;
    std::memcpy(&layout, GetRequestedStorage(ptr), sizeof(layout));
    return layout;
  }
  return Status::Unimplemented();
}

template <typename MetricsType>
Result<Layout> TrackingAllocator<MetricsType>::DoGetUsableLayout(
    const void* ptr) const {
  ptr = FromUsableSpace(ptr);
  Result<Layout> usable = GetUsableLayout(allocator_, ptr);
  if constexpr (has_requested_bytes<MetricsType>::value) {
    if (!allocator_.HasCapability(Capability::kImplementsGetRequestedLayout)) {
      if (usable.ok()) {
        size_t size = usable->size();
        PW_ASSERT(!PW_SUB_OVERFLOW(size, sizeof(Layout), &size));
        usable = Layout(size, usable->alignment());
      }
    }
  }
  return usable;
}

template <typename MetricsType>
Result<Layout> TrackingAllocator<MetricsType>::DoGetAllocatedLayout(
    const void* ptr) const {
  ptr = FromUsableSpace(ptr);
  return GetAllocatedLayout(allocator_, ptr);
}

template <typename MetricsType>
Layout TrackingAllocator<MetricsType>::ReserveStorage(Layout requested) {
  size_t size = requested.size();
  size_t alignment = requested.alignment();
  if constexpr (has_requested_bytes<MetricsType>::value) {
    if (!allocator_.HasCapability(Capability::kImplementsGetRequestedLayout)) {
      if (allocator_.HasCapability(Capability::kImplementsGetUsableLayout)) {
        PW_ASSERT(!PW_ADD_OVERFLOW(size, sizeof(Layout), &size));
      } else {
        alignment = std::max(alignment, sizeof(Layout));
        PW_ASSERT(!PW_ADD_OVERFLOW(alignment, size, &size));
      }
    }
  }
  return Layout(size, alignment);
}

template <typename MetricsType>
void* TrackingAllocator<MetricsType>::ToUsableSpace(
    void* ptr, const Layout& requested) const {
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  if constexpr (has_requested_bytes<MetricsType>::value) {
    if (!allocator_.HasCapability(Capability::kImplementsGetRequestedLayout) &&
        !allocator_.HasCapability(Capability::kImplementsGetUsableLayout)) {
      PW_ASSERT(PW_ADD_OVERFLOW(addr, requested.alignment(), &addr));
    }
  }
  return reinterpret_cast<void*>(addr);
}

template <typename MetricsType>
template <typename PtrType>
PtrType TrackingAllocator<MetricsType>::FromUsableSpace(PtrType ptr) const {
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  if constexpr (has_requested_bytes<MetricsType>::value) {
    if (!allocator_.HasCapability(Capability::kImplementsGetRequestedLayout) &&
        !allocator_.HasCapability(Capability::kImplementsGetUsableLayout)) {
      uintptr_t requested_addr;
      PW_ASSERT(PW_SUB_OVERFLOW(addr, sizeof(Layout), &requested_addr));
      Layout requested;
      std::memcpy(
          &requested, reinterpret_cast<void*>(requested_addr), sizeof(Layout));
      PW_ASSERT(PW_SUB_OVERFLOW(addr, requested.alignment(), &addr));
    }
  }
  return reinterpret_cast<PtrType>(addr);
}

template <typename MetricsType>
void TrackingAllocator<MetricsType>::SetRequested(void* ptr,
                                                  const Layout& requested) {
  if constexpr (has_requested_bytes<MetricsType>::value) {
    if (!allocator_.HasCapability(Capability::kImplementsGetRequestedLayout)) {
      std::memcpy(GetRequestedStorage(ptr), &requested, sizeof(requested));
    }
  }
}

template <typename MetricsType>
void* TrackingAllocator<MetricsType>::GetRequestedStorage(
    const void* ptr) const {
  if constexpr (has_requested_bytes<MetricsType>::value) {
    Result<Layout> usable = GetUsableLayout(allocator_, ptr);
    auto addr = reinterpret_cast<uintptr_t>(ptr);
    if (usable.ok()) {
      PW_ASSERT(!PW_ADD_OVERFLOW(addr, usable->size(), &addr));
      PW_ASSERT(!PW_SUB_OVERFLOW(addr, sizeof(Layout), &addr));
    } else {
      PW_ASSERT(!PW_ADD_OVERFLOW(addr, usable->alignment(), &addr));
      PW_ASSERT(!PW_SUB_OVERFLOW(addr, sizeof(Layout), &addr));
    }
    return reinterpret_cast<void*>(addr);
  } else {
    // `GetRequestedStorage()` is not available when `requested_bytes` disabled.
    PW_ASSERT(false);
  }
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
