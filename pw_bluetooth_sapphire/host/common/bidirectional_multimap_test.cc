// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/common/bidirectional_multimap.h"

#include "pw_unit_test/framework.h"

namespace bt {
namespace {

class BidirectionalMultimapTest : public testing::Test {
 protected:
  BidirectionalMultimap<int, char> map;
};

TEST_F(BidirectionalMultimapTest, EmptyContainerInvariants) {
  EXPECT_EQ(std::nullopt, map.get(0));
  EXPECT_EQ(std::nullopt, map.get('a'));

  EXPECT_EQ(false, map.contains('a'));
  EXPECT_EQ(false, map.contains(0));

  EXPECT_EQ(true, map.empty());
  EXPECT_EQ(0u, map.size_one());
  EXPECT_EQ(0u, map.size_many());
}

// insert one mapping, ensure we can get in both directions
TEST_F(BidirectionalMultimapTest, PutGetBothDirections) {
  map.put(0, 'a');

  ASSERT_NE(std::nullopt, map.get(0));
  EXPECT_EQ(1u, map.get(0)->get().count('a'));

  ASSERT_NE(std::nullopt, map.get('a'));
  EXPECT_EQ(0, map.get('a'));
}

// insert multiple mappings, ensure we can get in both directions
TEST_F(BidirectionalMultimapTest, ManyToOneRelationship) {
  map.put(0, 'a');
  map.put(0, 'b');

  ASSERT_NE(std::nullopt, map.get(0));
  EXPECT_EQ(1u, map.get(0)->get().count('a'));
  EXPECT_EQ(1u, map.get(0)->get().count('b'));

  ASSERT_NE(std::nullopt, map.get('a'));
  EXPECT_EQ(0, map.get('a'));
  ASSERT_NE(std::nullopt, map.get('b'));
  EXPECT_EQ(0, map.get('b'));
}

// remove one key, ensure all many keys also gets removed
TEST_F(BidirectionalMultimapTest, RemoveOne) {
  map.put(0, 'a');
  EXPECT_EQ(1u, map.size_one());

  map.remove(0);

  EXPECT_EQ(std::nullopt, map.get(0));
  EXPECT_EQ(std::nullopt, map.get('a'));
}

// remove a many key, ensure all other many keys don't also get removed
TEST_F(BidirectionalMultimapTest, RemoveMany) {
  map.put(0, 'a');
  map.put(0, 'b');
  EXPECT_EQ(1u, map.size_one());
  EXPECT_EQ(2u, map.size_many());

  map.remove('b');
  EXPECT_EQ(1u, map.size_many());

  EXPECT_NE(std::nullopt, map.get(0));
  EXPECT_NE(std::nullopt, map.get('a'));
  EXPECT_EQ(std::nullopt, map.get('b'));
}

// remove the last many key, ensure the one key gets removed as well
TEST_F(BidirectionalMultimapTest, RemoveLastMany) {
  map.put(0, 'a');
  EXPECT_EQ(1u, map.size_one());

  map.remove('a');
  EXPECT_EQ(0u, map.size_one());
  EXPECT_EQ(0u, map.size_many());

  EXPECT_EQ(std::nullopt, map.get(0));
  EXPECT_EQ(std::nullopt, map.get('a'));
}

// removing a nonexistent left does nothing
TEST_F(BidirectionalMultimapTest, RemoveNonExistent) {
  map.put(0, 'a');
  EXPECT_EQ(1u, map.size_one());

  map.remove(1);
  EXPECT_EQ(1u, map.size_one());
  EXPECT_EQ(1u, map.size_many());
  ASSERT_NE(std::nullopt, map.get(0));
  ASSERT_NE(std::nullopt, map.get('a'));
}

// insert one mapping, ensure container contains mappings from both left to
// right and right to left
TEST_F(BidirectionalMultimapTest, Contains) {
  map.put(0, 'a');

  EXPECT_TRUE(map.contains(0));
  EXPECT_TRUE(map.contains('a'));
}

// ensure empty reports correctly after addition and removal
TEST_F(BidirectionalMultimapTest, Empty) {
  EXPECT_TRUE(map.empty());

  map.put(0, 'a');
  EXPECT_FALSE(map.empty());

  map.remove(0);
  EXPECT_TRUE(map.empty());
}

// ensure clear removes all elements
TEST_F(BidirectionalMultimapTest, Clear) {
  map.put(0, 'a');
  map.put(1, 'b');
  EXPECT_EQ(2u, map.size_one());
  EXPECT_EQ(2u, map.size_many());

  map.clear();
  EXPECT_EQ(0u, map.size_one());
  EXPECT_EQ(0u, map.size_many());
  EXPECT_TRUE(map.empty());

  EXPECT_FALSE(map.contains('a'));
  EXPECT_FALSE(map.contains('b'));
  EXPECT_FALSE(map.contains(0));
  EXPECT_FALSE(map.contains(1));
}

}  // namespace
}  // namespace bt
