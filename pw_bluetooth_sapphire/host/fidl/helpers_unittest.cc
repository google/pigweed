// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helpers.h"

#include <gtest/gtest.h>

namespace bthost {
namespace fidl_helpers {
namespace {

TEST(FidlHelpersTest, AddressBytesFrommString) {
  EXPECT_FALSE(AddressBytesFromString(""));
  EXPECT_FALSE(AddressBytesFromString("FF"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:F"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FZ"));
  EXPECT_FALSE(AddressBytesFromString("FF:FF:FF:FF:FF:FF "));
  EXPECT_FALSE(AddressBytesFromString(" FF:FF:FF:FF:FF:FF"));

  auto addr1 = AddressBytesFromString("FF:FF:FF:FF:FF:FF");
  EXPECT_TRUE(addr1);
  EXPECT_EQ("FF:FF:FF:FF:FF:FF", addr1->ToString());

  auto addr2 = AddressBytesFromString("03:7F:FF:02:0F:01");
  EXPECT_TRUE(addr2);
  EXPECT_EQ("03:7F:FF:02:0F:01", addr2->ToString());
}

}  // namespace
}  // namespace fidl_helpers
}  // namespace bthost
