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

// DOCSTAG: [known_size_string]
#include "pw_string/string.h"

namespace examples {

static constexpr pw::InlineString<64> kMyString = [] {
  pw::InlineString<64> string;

  for (int i = 0; i < 5; ++i) {
    string += "Hello";
  }

  return string;
}();

}  // namespace examples
// DOCSTAG: [known_size_string]

namespace {

TEST(ExampleTests, KnownSizeString) {
  EXPECT_STREQ(examples::kMyString.c_str(), "HelloHelloHelloHelloHello");
}

}  // namespace
