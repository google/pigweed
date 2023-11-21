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

#include "pw_bluetooth_sapphire/internal/host/common/identifier.h"

#include <gtest/gtest.h>

#include <unordered_set>

namespace bt {
namespace {

constexpr Identifier<int> id1(1);
constexpr Identifier<int> id2(1);
constexpr Identifier<int> id3(2);

TEST(IdentifierTest, Equality) {
  EXPECT_EQ(id1, id2);
  EXPECT_NE(id2, id3);
  EXPECT_NE(id1, id3);
}

TEST(IdentifierTest, Hash) {
  std::unordered_set<Identifier<int>> ids;
  EXPECT_EQ(0u, ids.count(id1));
  EXPECT_EQ(0u, ids.size());

  ids.insert(id1);
  EXPECT_EQ(1u, ids.count(id1));
  EXPECT_EQ(1u, ids.size());

  ids.insert(id1);
  EXPECT_EQ(1u, ids.count(id1));
  EXPECT_EQ(1u, ids.size());

  ids.insert(id2);
  EXPECT_EQ(1u, ids.count(id1));
  EXPECT_EQ(1u, ids.size());

  ids.insert(id3);
  EXPECT_EQ(1u, ids.count(id2));
  EXPECT_EQ(2u, ids.size());

  ids.insert(id3);
  EXPECT_EQ(1u, ids.count(id2));
  EXPECT_EQ(2u, ids.size());
}

TEST(IdentifierTest, PeerIdIsValid) {
  {
    PeerId id;
    EXPECT_FALSE(id.IsValid());
  }

  {
    PeerId id(1);
    EXPECT_TRUE(id.IsValid());
  }
}

}  // namespace
}  // namespace bt
