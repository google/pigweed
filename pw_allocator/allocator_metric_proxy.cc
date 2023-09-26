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

Status AllocatorMetricProxy::DoQuery(const void* ptr,
                                     size_t size,
                                     size_t alignment) const {
  PW_DCHECK(allocator_ != nullptr);
  return allocator_->QueryUnchecked(ptr, size, alignment);
}

void* AllocatorMetricProxy::DoAllocate(size_t size, size_t alignment) {
  PW_DCHECK(allocator_ != nullptr);
  void* ptr = allocator_->AllocateUnchecked(size, alignment);
  if (ptr == nullptr) {
    return nullptr;
  }
  used_.Increment(size);
  if (used_.value() > peak_.value()) {
    peak_.Set(used_.value());
  }
  count_.Increment();
  return ptr;
}

void AllocatorMetricProxy::DoDeallocate(void* ptr,
                                        size_t size,
                                        size_t alignment) {
  PW_DCHECK(allocator_ != nullptr);
  PW_DCHECK(used_.value() >= size);
  PW_DCHECK(count_.value() != 0);
  allocator_->DeallocateUnchecked(ptr, size, alignment);
  used_.Set(used_.value() - size);
  count_.Set(count_.value() - 1);
}

bool AllocatorMetricProxy::DoResize(void* ptr,
                                    size_t old_size,
                                    size_t old_alignment,
                                    size_t new_size) {
  PW_DCHECK(allocator_ != nullptr);
  PW_DCHECK(used_.value() >= old_size);
  if (!allocator_->ResizeUnchecked(ptr, old_size, old_alignment, new_size)) {
    return false;
  }
  used_.Set(used_.value() - old_size + new_size);
  if (used_.value() > peak_.value()) {
    peak_.Set(used_.value());
  }
  return true;
}

}  // namespace pw::allocator
