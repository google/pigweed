// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "identifier.h"

#include <unordered_set>

#include "gtest/gtest.h"

namespace btlib {
namespace common {
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

TEST(IdentifierTest, DeviceIdIsValid) {
  {
    DeviceId id;
    EXPECT_FALSE(id.IsValid());
  }

  {
    DeviceId id(1);
    EXPECT_TRUE(id.IsValid());
  }
}

}  // namespace
}  // namespace common
}  // namespace btlib
