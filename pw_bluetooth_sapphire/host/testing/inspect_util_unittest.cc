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

#include "pw_bluetooth_sapphire/internal/host/testing/inspect_util.h"

#ifndef NINSPECT

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bt::testing {

using ::testing::Optional;

TEST(InspectUtil, InspectPropertyValueAtPathSuccess) {
  inspect::Inspector inspector_;
  inspect::Node child = inspector_.GetRoot().CreateChild("child");
  inspect::IntProperty prop = child.CreateInt("property", 42);
  EXPECT_THAT(GetInspectValue<inspect::IntPropertyValue>(inspector_,
                                                         {"child", "property"}),
              Optional(42));
}

TEST(InspectUtil, InspectPropertyValueAtPathFailure) {
  inspect::Inspector inspector_;
  inspect::Node child = inspector_.GetRoot().CreateChild("child");
  EXPECT_FALSE(GetInspectValue<inspect::StringPropertyValue>(
      inspector_, {"child", "property"}));
}

TEST(InspectUtil, EmptyPath) {
  inspect::Inspector inspector_;
  EXPECT_FALSE(GetInspectValue<inspect::IntPropertyValue>(inspector_, {}));
}

TEST(InspectUtil, NodeInPathDoesNotExist) {
  inspect::Inspector inspector_;
  EXPECT_FALSE(GetInspectValue<inspect::StringPropertyValue>(
      inspector_, {"child", "property"}));
}

}  // namespace bt::testing

#endif  // NINSPECT
