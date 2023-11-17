// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_INSPECT_UTIL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_INSPECT_UTIL_H_

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

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_INSPECT_UTIL_H_
