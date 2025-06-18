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

#define PW_LOG_MODULE_NAME "PW_STRING_EXAMPLES"

#include "pw_unit_test/framework.h"

// DOCSTAG[start-pw_string-buffer-example]
#include "pw_log/log.h"
#include "pw_span/span.h"
#include "pw_string/string_builder.h"

namespace examples {

void BuildString(pw::StringBuilder& sb) {
  // Add to the builder with idiomatic C++.
  sb << "Is it really this easy?";
  sb << " YES!";

  // Use the builder like any other string.
  PW_LOG_DEBUG("%s", sb.c_str());
}

void BuildStringDemo() {
  // Create a builder with a built-in buffer.
  std::byte buffer[64];
  pw::StringBuilder sb(buffer);
  BuildString(sb);
}

}  // namespace examples
// DOCSTAG[end-pw_string-buffer-example]

namespace {

TEST(StringExamples, BufferExample) {
  std::byte buffer[64];
  pw::StringBuilder sb(buffer);
  examples::BuildString(sb);
  auto expected = "Is it really this easy? YES!";
  EXPECT_STREQ(sb.c_str(), expected);
}

}  // namespace
