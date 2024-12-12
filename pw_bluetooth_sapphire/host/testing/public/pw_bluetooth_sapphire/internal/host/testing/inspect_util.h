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
#include <gmock/gmock.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/testing/inspect.h"

#ifndef NINSPECT

namespace bt::testing {

// Read the hierarchy of |inspector|.
inspect::Hierarchy ReadInspect(const inspect::Inspector& inspector);

// Get the value of the property at |path|. The last item in |path|
// should be the property name.
// Example:
// EXPECT_THAT(GetInspectValue<inspect::IntPropertyValue>(inspector, {"node",
// "property"}),
//             Optional(42));
template <class PropertyValue>
std::optional<typename PropertyValue::value_type> GetInspectValue(
    const inspect::Inspector& inspector, std::vector<std::string> path) {
  if (path.empty()) {
    return std::nullopt;
  }

  // The last path item is the property name.
  std::string property = path.back();
  path.pop_back();

  inspect::Hierarchy hierarchy = ReadInspect(inspector);
  auto node = hierarchy.GetByPath(path);
  if (!node) {
    return std::nullopt;
  }
  const PropertyValue* prop_value =
      node->node().get_property<PropertyValue>(property);
  if (!prop_value) {
    return std::nullopt;
  }
  return prop_value->value();
}

}  // namespace bt::testing

#endif  // NINSPECT
