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

#include "pw_log_tokenized/fields.h"

#include <array>
#include <string_view>
#include <utility>

#include "pw_status/status_with_size.h"
#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::Status;
using pw::StatusWithSize;
using pw::log_tokenized::ParseFields;

struct Field {
  std::string_view key;
  std::string_view value;
};

PW_CONSTEXPR_TEST(ParseFields, EmptyString, {
  size_t fields_parsed = 0;
  StatusWithSize result =
      ParseFields("", [&fields_parsed](std::string_view, std::string_view) {
        fields_parsed += 1;
      });
  PW_TEST_EXPECT_OK(result.status());
  PW_TEST_EXPECT_EQ(0u, result.size());
  PW_TEST_EXPECT_EQ(0u, fields_parsed);
});

PW_CONSTEXPR_TEST(ParseFields, NoFields, {
  size_t fields_parsed = 0;
  StatusWithSize result = ParseFields(
      "Hello world", [&fields_parsed](std::string_view, std::string_view) {
        fields_parsed += 1;
      });
  PW_TEST_EXPECT_OK(result.status());
  PW_TEST_EXPECT_EQ(0u, result.size());
  PW_TEST_EXPECT_EQ(0u, fields_parsed);
});

PW_CONSTEXPR_TEST(ParseFields, OneField, {
  std::array<Field, 1> fields;
  size_t fields_parsed = 0;
  StatusWithSize result = ParseFields(
      "■msg♦Hello world",
      [&fields, &fields_parsed](std::string_view key, std::string_view val) {
        fields[fields_parsed++] = {key, val};
      });
  PW_TEST_EXPECT_OK(result.status());
  PW_TEST_ASSERT_EQ(1u, result.size());
  PW_TEST_ASSERT_EQ(1u, fields_parsed);
  PW_TEST_EXPECT_EQ("msg", fields[0].key);
  PW_TEST_EXPECT_EQ("Hello world", fields[0].value);
});

PW_CONSTEXPR_TEST(ParseFields, MultipleFields, {
  std::array<Field, 3> fields;
  size_t fields_parsed = 0;
  StatusWithSize result = ParseFields(
      "■msg♦Hello■module♦test■file♦test.cc",
      [&fields, &fields_parsed](std::string_view key, std::string_view val) {
        fields[fields_parsed++] = {key, val};
      });
  PW_TEST_EXPECT_OK(result.status());
  PW_TEST_ASSERT_EQ(3u, result.size());
  PW_TEST_ASSERT_EQ(3u, fields_parsed);
  PW_TEST_EXPECT_EQ("msg", fields[0].key);
  PW_TEST_EXPECT_EQ("Hello", fields[0].value);
  PW_TEST_EXPECT_EQ("module", fields[1].key);
  PW_TEST_EXPECT_EQ("test", fields[1].value);
  PW_TEST_EXPECT_EQ("file", fields[2].key);
  PW_TEST_EXPECT_EQ("test.cc", fields[2].value);
});

PW_CONSTEXPR_TEST(ParseFields, IncompleteField, {
  size_t fields_parsed = 0;
  StatusWithSize result =
      ParseFields("■msg", [&fields_parsed](std::string_view, std::string_view) {
        fields_parsed += 1;
      });
  PW_TEST_EXPECT_EQ(Status::DataLoss(), result.status());
  PW_TEST_EXPECT_EQ(0u, result.size());
  PW_TEST_EXPECT_EQ(0u, fields_parsed);
});

PW_CONSTEXPR_TEST(ParseFields, IncompleteFieldAfterOne, {
  std::array<Field, 2> fields;
  size_t fields_parsed = 0;
  StatusWithSize result = ParseFields(
      "■msg♦Hello■key2",
      [&fields, &fields_parsed](std::string_view key, std::string_view val) {
        fields[fields_parsed++] = {key, val};
      });
  PW_TEST_EXPECT_EQ(Status::DataLoss(), result.status());
  PW_TEST_EXPECT_EQ(1u, result.size());
  PW_TEST_ASSERT_EQ(1u, fields_parsed);
  PW_TEST_EXPECT_EQ("msg", fields[0].key);
  PW_TEST_EXPECT_EQ("Hello", fields[0].value);
});

PW_CONSTEXPR_TEST(ParseFields, EmptyValue, {
  std::array<Field, 1> fields;
  size_t fields_parsed = 0;
  StatusWithSize result = ParseFields(
      "■msg♦",
      [&fields, &fields_parsed](std::string_view key, std::string_view val) {
        fields[fields_parsed++] = {key, val};
      });
  PW_TEST_EXPECT_OK(result.status());
  PW_TEST_ASSERT_EQ(1u, result.size());
  PW_TEST_ASSERT_EQ(1u, fields_parsed);
  PW_TEST_EXPECT_EQ("msg", fields[0].key);
  PW_TEST_EXPECT_EQ("", fields[0].value);
});

PW_CONSTEXPR_TEST(ParseFields, EmptyKey, {
  std::array<Field, 1> fields;
  size_t fields_parsed = 0;
  StatusWithSize result = ParseFields(
      "■♦value",
      [&fields, &fields_parsed](std::string_view key, std::string_view val) {
        fields[fields_parsed++] = {key, val};
      });
  PW_TEST_EXPECT_OK(result.status());
  PW_TEST_ASSERT_EQ(1u, result.size());
  PW_TEST_ASSERT_EQ(1u, fields_parsed);
  PW_TEST_EXPECT_EQ("", fields[0].key);
  PW_TEST_EXPECT_EQ("value", fields[0].value);
});

PW_CONSTEXPR_TEST(ParseFields, CustomSeparators, {
  std::array<Field, 2> fields;
  size_t fields_parsed = 0;
  StatusWithSize result = ParseFields(
      "$key1:value1$key2:value2",
      [&fields, &fields_parsed](std::string_view key, std::string_view val) {
        fields[fields_parsed++] = {key, val};
      },
      "$",
      ":");
  PW_TEST_EXPECT_OK(result.status());
  PW_TEST_ASSERT_EQ(2u, result.size());
  PW_TEST_ASSERT_EQ(2u, fields_parsed);
  PW_TEST_EXPECT_EQ("key1", fields[0].key);
  PW_TEST_EXPECT_EQ("value1", fields[0].value);
  PW_TEST_EXPECT_EQ("key2", fields[1].key);
  PW_TEST_EXPECT_EQ("value2", fields[1].value);
});

}  // namespace
