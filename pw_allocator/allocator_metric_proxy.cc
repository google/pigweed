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

#include "pw_allocator/allocator_metric_proxy.h"

#include <cstddef>

#include "pw_assert/check.h"
#include "pw_metric/metric.h"

namespace pw::allocator {

void AllocatorMetricProxy::Initialize(Allocator& allocator) {
  PW_DCHECK(allocator_ == nullptr);
  allocator_ = &allocator;
  // Manually add the metrics to the metric group to allow the constructor to
  // remain constexpr.
  memusage_.Add(used_);
  memusage_.Add(peak_);
  memusage_.Add(count_);
}

Status AllocatorMetricProxy::DoQuery(const void* ptr, Layout layout) const {
  PW_DCHECK_NOTNULL(allocator_);
  return allocator_->Query(ptr, layout);
}

void* AllocatorMetricProxy::DoAllocate(Layout layout) {
  PW_DCHECK_NOTNULL(allocator_);
  void* ptr = allocator_->Allocate(layout);
  if (ptr == nullptr) {
    return nullptr;
  }
  used_.Increment(layout.size());
  if (used_.value() > peak_.value()) {
    peak_.Set(used_.value());
  }
  count_.Increment();
  return ptr;
}

void AllocatorMetricProxy::DoDeallocate(void* ptr, Layout layout) {
  PW_DCHECK_NOTNULL(allocator_);
  allocator_->Deallocate(ptr, layout);
  if (ptr == nullptr) {
    return;
  }
  PW_DCHECK_UINT_GE(used_.value(), layout.size());
  PW_DCHECK_UINT_NE(count_.value(), 0);
  used_.Set(used_.value() - layout.size());
  count_.Set(count_.value() - 1);
}

bool AllocatorMetricProxy::DoResize(void* ptr, Layout layout, size_t new_size) {
  PW_DCHECK_NOTNULL(allocator_);
  if (!allocator_->Resize(ptr, layout, new_size)) {
    return false;
  }
  PW_DCHECK_UINT_GE(used_.value(), layout.size());
  used_.Set(used_.value() - layout.size() + new_size);
  if (used_.value() > peak_.value()) {
    peak_.Set(used_.value());
  }
  return true;
}

}  // namespace pw::allocator
