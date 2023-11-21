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

#include "pw_bluetooth_sapphire/internal/host/testing/parse_args.h"

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
