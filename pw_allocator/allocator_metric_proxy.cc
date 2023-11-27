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

namespace pw::allocator::internal {

void Metrics::Init() {
  Add(used_);
  Add(peak_);
  Add(count_);
}

void Metrics::Update(size_t old_size, size_t new_size) {
  size_t used = used_.value();
  size_t count = count_.value();
  if (old_size != 0) {
    PW_DCHECK_UINT_GE(used, old_size);
    PW_DCHECK_UINT_GT(count, 0);
    used -= old_size;
    --count;
  }
  if (new_size != 0) {
    used += new_size;
    ++count;
    PW_DCHECK_UINT_GE(used, new_size);
    PW_DCHECK_UINT_GT(count, 0);
  }
  if (used > peak_.value()) {
    peak_.Set(used);
  }
  used_.Set(used);
  count_.Set(count);
}

}  // namespace pw::allocator::internal
