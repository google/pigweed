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

// DOCSTAG: [contributing-docs-examples]
#define PW_LOG_MODULE_NAME "EXAMPLES_BUILD_STRING_IN_BUFFER"

#include "pw_unit_test/framework.h"

// DOCSTAG: [build-string-in-buffer]
#include "pw_log/log.h"
#include "pw_string/string_builder.h"

namespace examples {

void BuildStringInBuffer(pw::StringBuilder& sb) {
  // Add to the builder with idiomatic C++.
  sb << "Is it really this easy?";
  sb << " YES!";

  // Use the builder like any other string.
  PW_LOG_DEBUG("%s", sb.c_str());
}

void main() {
  // Create a builder with a built-in buffer.
  std::byte buffer[64];
  pw::StringBuilder sb(buffer);
  BuildStringInBuffer(sb);
}

}  // namespace examples
// DOCSTAG: [build-string-in-buffer]

namespace {

TEST(ExampleTests, BuildStringInBufferTest) {
  examples::main();  // Call the secondary example, just for coverage.
  std::byte buffer[64];
  pw::StringBuilder sb(buffer);
  examples::BuildStringInBuffer(sb);
  const char* expected = "Is it really this easy? YES!";
  const char* actual = sb.c_str();
  EXPECT_STREQ(expected, actual);
}

}  // namespace
// DOCSTAG: [contributing-docs-examples]
