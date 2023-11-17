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

#include "pw_bluetooth_sapphire/internal/host/common/bounded_inspect_list_node.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/testing/inspect.h"

#ifndef NINSPECT

namespace bt {

namespace {

using namespace ::inspect::testing;
using Item = BoundedInspectListNode::Item;

TEST(BoundedInspectListNodeTest, ListEviction) {
  const size_t kCapacity = 2;
  inspect::Inspector inspector;
  BoundedInspectListNode list(kCapacity);

  list.AttachInspect(inspector.GetRoot(), "list_name");
  Item& item_0 = list.CreateItem();
  item_0.node.RecordInt("item_0", 0);
  Item& item_1 = list.CreateItem();
  item_1.node.RecordInt("item_1", 1);

  auto hierarchy = ::inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy.is_ok());
  EXPECT_THAT(
      hierarchy.take_value(),
      ChildrenMatch(ElementsAre(AllOf(
          // list node
          NodeMatches(NameMatches("list_name")),
          ChildrenMatch(UnorderedElementsAre(
              // item_0
              NodeMatches(AllOf(NameMatches("0"),
                                PropertyList(ElementsAre(IntIs("item_0", 0))))),
              // item_1
              NodeMatches(
                  AllOf(NameMatches("1"),
                        PropertyList(ElementsAre(IntIs("item_1", 1)))))))))));

  // Exceed capacity. item_0 should be evicted.
  Item& item_2 = list.CreateItem();
  item_2.node.RecordInt("item_2", 2);

  hierarchy = ::inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy.is_ok());
  EXPECT_THAT(
      hierarchy.take_value(),
      ChildrenMatch(ElementsAre(AllOf(
          // list node
          NodeMatches(NameMatches("list_name")),
          ChildrenMatch(UnorderedElementsAre(
              // item_1
              NodeMatches(AllOf(NameMatches("1"),
                                PropertyList(ElementsAre(IntIs("item_1", 1))))),
              // item_2
              NodeMatches(
                  AllOf(NameMatches("2"),
                        PropertyList(ElementsAre(IntIs("item_2", 2)))))))))));
}

}  // namespace
}  // namespace bt

#endif  // NINSPECT
