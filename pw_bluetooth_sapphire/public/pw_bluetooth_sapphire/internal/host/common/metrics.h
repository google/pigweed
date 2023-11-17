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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_METRICS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_METRICS_H_

#include "pw_bluetooth_sapphire/internal/host/common/inspect.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"

namespace bt {

// Represents a metric counter.
template <typename property_t>
class MetricCounter {
 public:
  MetricCounter() = default;
  virtual ~MetricCounter() = default;

  // Attach peer inspect node as a child node of |parent|.
  virtual void AttachInspect(inspect::Node& parent, const std::string& name);

  // Increment the metrics counter by |value|.
  void Add(int value = 1) { inspect_property_.Add(value); }

  // Decrement the metrics counter by |value|.
  void Subtract(int value = 1) { inspect_property_.Subtract(value); }

 private:
  // Node property
  property_t inspect_property_;

  // Update underlying inspect attributes.
  void UpdateInspect();

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(MetricCounter);
};

using IntMetricCounter = MetricCounter<inspect::IntProperty>;
using UintMetricCounter = MetricCounter<inspect::UintProperty>;

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_METRICS_H_
