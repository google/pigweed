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

#include "pw_tokenizer_private/csv.h"

#include "pw_unit_test/framework.h"

namespace {

using ::pw::tokenizer::ParseCsv;

TEST(CsvParser, ReadFile) {
  static constexpr const char kCsv[] =
      "abc,def,ghi\n"
      "\"\",\"\"\"\",\n"
      "123,\"\",\"4\"";

  const auto result = ParseCsv(kCsv);
  EXPECT_EQ(result.size(), 3u);

  EXPECT_EQ(result[0][0], "abc");
  EXPECT_EQ(result[0][1], "def");
  EXPECT_EQ(result[0][2], "ghi");
  EXPECT_EQ(result[1][0], "");
  EXPECT_EQ(result[1][1], "\"");
  EXPECT_EQ(result[1][2], "");
  EXPECT_EQ(result[2][0], "123");
  EXPECT_EQ(result[2][1], "");
  EXPECT_EQ(result[2][2], "4");
}

TEST(CsvParser, EmptyLines) {
  static constexpr const char kCsv[] = "\n\n\r\n\r\n";
  const auto result = ParseCsv(kCsv);
  EXPECT_EQ(result.size(), 0u);
}

TEST(CsvParser, EmptyLinesWithTextInterspersed) {
  static constexpr const char kCsv[] = "\n\n\r \n\r\n\r\n\r,\r\n";
  const auto result = ParseCsv(kCsv);
  ASSERT_EQ(result.size(), 2u);
  ASSERT_EQ(result[0].size(), 1u);
  EXPECT_EQ(result[0][0], " ");

  ASSERT_EQ(result[1].size(), 2u);
  EXPECT_EQ(result[1][0], "");
  EXPECT_EQ(result[1][1], "");
}

TEST(CsvParser, VaryingColumns) {
  static constexpr const char kCsv[] =
      "\n"
      "a\r\n"
      "b\r\n"
      ",\r\n"
      "c,d,,\r\n"
      " , ,\"\n\"\n"
      "e,fg,hijk,lmno ";

  const std::vector<std::vector<std::string>> kExpected = {
      {"a"},
      {"b"},
      {"", ""},
      {"c", "d", "", ""},
      {" ", " ", "\n"},
      {"e", "fg", "hijk", "lmno "}};
  const auto result = ParseCsv(kCsv);
  ASSERT_EQ(result.size(), 6u);
  EXPECT_EQ(kExpected, result);
}

TEST(CsvParser, Error_NoLines) {
  const auto result = ParseCsv(R"(11,"abc"., 13 )");
  ASSERT_EQ(result.size(), 0u);
}

TEST(CsvParser, Error_SkipsOnlyErrors) {
  const auto result = ParseCsv(
      "a,b,c\n"
      "1,\"2\".,3\n"
      "d,e\r\n"
      "\"456\n\r\" 789\r\n\r\n"
      "f,g,h\n"
      "\"0");
  const std::vector<std::vector<std::string>> kExpected = {
      {"a", "b", "c"},
      {"d", "e"},
      {"f", "g", "h"},
  };
  EXPECT_EQ(result, kExpected);
}

}  // namespace
