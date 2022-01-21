// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/testing/inspect_util.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bt::testing {

using ::testing::Optional;

TEST(InspectUtil, InspectPropertyValueAtPathSuccess) {
  inspect::Inspector inspector_;
  inspect::Node child = inspector_.GetRoot().CreateChild("child");
  inspect::IntProperty prop = child.CreateInt("property", 42);
  EXPECT_THAT(GetInspectValue<inspect::IntPropertyValue>(inspector_, {"child", "property"}),
              Optional(42));
}

TEST(InspectUtil, InspectPropertyValueAtPathFailure) {
  inspect::Inspector inspector_;
  inspect::Node child = inspector_.GetRoot().CreateChild("child");
  EXPECT_FALSE(GetInspectValue<inspect::StringPropertyValue>(inspector_, {"child", "property"}));
}

TEST(InspectUtil, EmptyPath) {
  inspect::Inspector inspector_;
  EXPECT_FALSE(GetInspectValue<inspect::IntPropertyValue>(inspector_, {}));
}

TEST(InspectUtil, NodeInPathDoesNotExist) {
  inspect::Inspector inspector_;
  EXPECT_FALSE(GetInspectValue<inspect::StringPropertyValue>(inspector_, {"child", "property"}));
}

}  // namespace bt::testing
