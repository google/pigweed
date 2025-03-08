// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/common/bidirectional_map.h"

#include "pw_unit_test/framework.h"

namespace bt {
namespace {

template <typename T>
class BidirectionalMapTest : public testing::Test {
 protected:
  T map;
};

using Implementations =
    testing::Types<BidirectionalMap<int, char>, BidirectionalMap<char, int>>;

TYPED_TEST_SUITE(BidirectionalMapTest, Implementations);

// invariant checks on an empty container
TYPED_TEST(BidirectionalMapTest, EmptyContainerInvariants) {
  EXPECT_EQ(std::nullopt, this->map.get('a'));
  EXPECT_EQ(std::nullopt, this->map.get(0));

  EXPECT_EQ(false, this->map.contains('a'));
  EXPECT_EQ(false, this->map.contains(0));

  EXPECT_EQ(true, this->map.empty());
  EXPECT_EQ(0u, this->map.size());
}

// insert one mapping, ensure we can get in both directions
TYPED_TEST(BidirectionalMapTest, InsertGetBothDirections) {
  this->map.insert(0, 'a');

  ASSERT_NE(std::nullopt, this->map.get(0));
  EXPECT_EQ('a', *(this->map.get(0)));

  ASSERT_NE(std::nullopt, this->map.get('a'));
  EXPECT_EQ(0, *(this->map.get('a')));
}

// change in left overwrites mapping mapping in both directions
TYPED_TEST(BidirectionalMapTest, InsertOverwrite) {
  this->map.insert(0, 'a');
  EXPECT_EQ(1u, this->map.size());

  this->map.insert(1, 'a');
  EXPECT_EQ(1u, this->map.size());

  EXPECT_FALSE(this->map.contains(0));
  EXPECT_EQ(std::nullopt, this->map.get(0));

  ASSERT_NE(std::nullopt, this->map.get(1));
  EXPECT_EQ('a', *(this->map.get(1)));

  ASSERT_NE(std::nullopt, this->map.get('a'));
  EXPECT_EQ(1, *(this->map.get('a')));
}

// erase the left, ensure right also gets erased
TYPED_TEST(BidirectionalMapTest, Erase) {
  this->map.insert(0, 'a');
  EXPECT_EQ(1u, this->map.size());

  this->map.erase(0);

  EXPECT_EQ(std::nullopt, this->map.get(0));
  EXPECT_EQ(std::nullopt, this->map.get('a'));
}

// removing a nonexistent left does nothing
TYPED_TEST(BidirectionalMapTest, EraseNonExistent) {
  this->map.insert(0, 'a');
  EXPECT_EQ(1u, this->map.size());

  this->map.erase(1);
  EXPECT_EQ(1u, this->map.size());
  ASSERT_NE(std::nullopt, this->map.get(0));
  EXPECT_EQ('a', *(this->map.get(0)));
}

// insert one mapping, ensure container contains mappings from both left to
// right and right to left
TYPED_TEST(BidirectionalMapTest, Contains) {
  this->map.insert(0, 'a');

  EXPECT_TRUE(this->map.contains(0));
  EXPECT_TRUE(this->map.contains('a'));
}

// ensure empty reports correctly after addition and removal
TYPED_TEST(BidirectionalMapTest, Empty) {
  EXPECT_TRUE(this->map.empty());

  this->map.insert(0, 'a');
  EXPECT_FALSE(this->map.empty());

  this->map.erase(0);
  EXPECT_TRUE(this->map.empty());
}

// ensure clear erases all elements
TYPED_TEST(BidirectionalMapTest, Clear) {
  this->map.insert(0, 'a');
  this->map.insert(1, 'b');
  EXPECT_EQ(2u, this->map.size());

  this->map.clear();
  EXPECT_EQ(0u, this->map.size());
  EXPECT_TRUE(this->map.empty());

  EXPECT_FALSE(this->map.contains('a'));
  EXPECT_FALSE(this->map.contains('b'));
  EXPECT_FALSE(this->map.contains(0));
  EXPECT_FALSE(this->map.contains(1));
}

}  // namespace
}  // namespace bt
