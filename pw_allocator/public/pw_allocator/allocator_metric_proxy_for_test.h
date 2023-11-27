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

#include "pw_allocator/allocator_metric_proxy.h"

namespace pw::allocator {

/// Allocator metric proxy that always uses the real metrics implementation.
///
/// This class always collects metrics, regardless of the value of the
/// `pw_allocator_COLLECT_METRICS` build argument, making it useful for unit
/// tests which rely on metric collection.
class AllocatorMetricProxyForTest
    : public AllocatorMetricProxyImpl<internal::Metrics> {
 public:
  constexpr explicit AllocatorMetricProxyForTest(metric::Token token)
      : AllocatorMetricProxyImpl(token) {}
};

}  // namespace pw::allocator
