// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "parse_args.h"

#include <string>
#include <string_view>

#include "gtest/gtest.h"

namespace bt::testing {
namespace {

TEST(ParseArgsTest, GetArgValueNoHyphens) {
  int argc = 2;
  std::string arg0("test");
  std::string arg1("key=value");
  char* argv[] = {arg0.data(), arg1.data()};
  EXPECT_FALSE(GetArgValue("key", argc, argv));
}

TEST(ParseArgsTest, GetArgValueNoValue) {
  int argc = 2;
  std::string arg0("test");
  std::string arg1("--key");
  char* argv[] = {arg0.data(), arg1.data()};
  EXPECT_FALSE(GetArgValue("key", argc, argv));
}

TEST(ParseArgsTest, GetArgValueEmptyValue) {
  int argc = 2;
  std::string arg0("test");
  std::string arg1("--key=");
  char* argv[] = {arg0.data(), arg1.data()};
  std::optional<std::string_view> value = GetArgValue("key", argc, argv);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->size(), 0u);
}

TEST(ParseArgsTest, GetArgValueSuccess) {
  int argc = 2;
  std::string arg0("test");
  std::string arg1("--key=value");
  std::string expected_value("value");
  char* argv[] = {arg0.data(), arg1.data()};
  std::optional<std::string_view> value = GetArgValue("key", argc, argv);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), expected_value);
}

TEST(ParseArgsTest, GetArgValueMultipleArgs) {
  int argc = 3;
  std::string arg0("test");
  std::string arg1("--abc=def");
  std::string arg2("--key=value");
  std::string expected_value("value");
  char* argv[] = {arg0.data(), arg1.data(), arg2.data()};
  std::optional<std::string_view> value = GetArgValue("key", argc, argv);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), expected_value);
}

}  // namespace
}  // namespace bt::testing
