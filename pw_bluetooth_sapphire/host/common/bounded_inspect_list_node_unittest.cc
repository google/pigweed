// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bounded_inspect_list_node.h"

#include <lib/inspect/testing/cpp/inspect.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"

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
  item_0.node.CreateInt("item_0", 0, &item_0.values);
  Item& item_1 = list.CreateItem();
  item_1.node.CreateInt("item_1", 1, &item_1.values);

  auto hierarchy = ::inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy.is_ok());
  EXPECT_THAT(
      hierarchy.take_value(),
      ChildrenMatch(ElementsAre(AllOf(
          // list node
          NodeMatches(NameMatches("list_name")),
          ChildrenMatch(UnorderedElementsAre(
              // item_0
              NodeMatches(AllOf(NameMatches("0"), PropertyList(ElementsAre(IntIs("item_0", 0))))),
              // item_1
              NodeMatches(
                  AllOf(NameMatches("1"), PropertyList(ElementsAre(IntIs("item_1", 1)))))))))));

  // Exceed capacity. item_0 should be evicted.
  Item& item_2 = list.CreateItem();
  item_2.node.CreateInt("item_2", 2, &item_2.values);

  hierarchy = ::inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy.is_ok());
  EXPECT_THAT(
      hierarchy.take_value(),
      ChildrenMatch(ElementsAre(AllOf(
          // list node
          NodeMatches(NameMatches("list_name")),
          ChildrenMatch(UnorderedElementsAre(
              // item_1
              NodeMatches(AllOf(NameMatches("1"), PropertyList(ElementsAre(IntIs("item_1", 1))))),
              // item_2
              NodeMatches(
                  AllOf(NameMatches("2"), PropertyList(ElementsAre(IntIs("item_2", 2)))))))))));
}

}  // namespace
}  // namespace bt
