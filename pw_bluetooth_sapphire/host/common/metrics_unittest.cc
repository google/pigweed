// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/common/metrics.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/testing/inspect.h"

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
