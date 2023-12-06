// Copyright 2019 The Pigweed Authors
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

#include <cstring>

#include "pw_status/status.h"
#include "pw_string/string_builder.h"
#include "pw_unit_test/framework.h"

namespace pw {
namespace {

TEST(UnknownTypeToString, SmallObject) {
  struct {
    char a = 0xa1;
  } object;

  StringBuffer<64> expected;
  expected << "<1-byte object at 0x" << &object << '>';
  ASSERT_EQ(OkStatus(), expected.status());

  StringBuffer<64> actual;
  actual << object;
  ASSERT_EQ(OkStatus(), actual.status());
  EXPECT_STREQ(expected.c_str(), actual.c_str());
}

TEST(UnknownTypeToString, NineByteObject) {
  struct {
    char a[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  } object;

  StringBuffer<64> expected;
  expected << "<9-byte object at 0x" << &object << '>';
  ASSERT_EQ(OkStatus(), expected.status());

  StringBuffer<64> actual;
  actual << object;
  ASSERT_EQ(OkStatus(), actual.status());
  EXPECT_STREQ(expected.c_str(), actual.c_str());
}

TEST(UnknownTypeToString, TenByteObject) {
  struct {
    char a[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  } object;

  StringBuffer<72> expected;
  expected << "<10-byte object at 0x" << &object << '>';
  ASSERT_EQ(OkStatus(), expected.status());

  StringBuffer<72> actual;
  actual << object;
  ASSERT_EQ(OkStatus(), actual.status());
  EXPECT_STREQ(expected.c_str(), actual.c_str());
}

}  // namespace
}  // namespace pw
