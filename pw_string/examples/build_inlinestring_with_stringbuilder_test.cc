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

#include "pw_unit_test/framework.h"

// DOCSTAG: [build-inlinestring-with-stringbuilder]
#include "pw_assert/check.h"
#include "pw_string/string.h"
#include "pw_string/string_builder.h"

namespace examples {

void BuildInlineStringWithStringBuilder(pw::InlineString<32>& is) {
  pw::StringBuilder sb(is);
  sb << 123 << "456";
  PW_CHECK(std::string_view(sb) == "123456");
}

void main() {
  pw::InlineString<32> is;
  BuildInlineStringWithStringBuilder(is);
}

}  // namespace examples
// DOCSTAG: [build-inlinestring-with-stringbuilder]

namespace {

TEST(ExampleTests, BuildInlineStringWithStringBuilderTest) {
  examples::main();  // Call the secondary example, just for coverage.
  pw::InlineString<32> is;
  examples::BuildInlineStringWithStringBuilder(is);
  const char* actual = is.c_str();
  const char* expected = "123456";
  EXPECT_STREQ(actual, expected);
}

}  // namespace
