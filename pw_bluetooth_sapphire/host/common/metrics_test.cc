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

#include "pw_bluetooth_sapphire/internal/host/common/metrics.h"

#include "pw_bluetooth_sapphire/internal/host/testing/inspect.h"
#include "pw_unit_test/framework.h"

#ifndef NINSPECT

namespace bt {
namespace {

using namespace inspect::testing;

TEST(MetrictsTest, PropertyAddSubInt) {
  inspect::Inspector inspector;
  auto counter = UintMetricCounter();
  auto child = inspector.GetRoot().CreateChild("child");
  counter.AttachInspect(child, "value");

  auto node_matcher_0 = AllOf(NodeMatches(
      AllOf(NameMatches("child"),
            PropertyList(UnorderedElementsAre(UintIs("value", 0))))));

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo()).take_value();
  EXPECT_THAT(hierarchy,
              AllOf(ChildrenMatch(UnorderedElementsAre(node_matcher_0))));

  counter.Add(5);

  auto node_matcher_1 = AllOf(NodeMatches(
      AllOf(NameMatches("child"),
            PropertyList(UnorderedElementsAre(UintIs("value", 5))))));

  hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo()).take_value();
  EXPECT_THAT(hierarchy,
              AllOf(ChildrenMatch(UnorderedElementsAre(node_matcher_1))));

  counter.Subtract();

  auto node_matcher_2 = AllOf(NodeMatches(
      AllOf(NameMatches("child"),
            PropertyList(UnorderedElementsAre(UintIs("value", 4))))));

  hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo()).take_value();
  EXPECT_THAT(hierarchy,
              AllOf(ChildrenMatch(UnorderedElementsAre(node_matcher_2))));
}

}  // namespace

}  // namespace bt

#endif  // NINSPECT
