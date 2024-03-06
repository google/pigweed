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

#pragma once

#include <cstddef>
#include <string_view>

#include "pw_preprocessor/compiler.h"
#include "pw_unit_test/event_handler.h"
#include "pw_unit_test/googletest_style_event_handler.h"

namespace pw::unit_test {

/// Predefined event handler implementation that produces human-readable
/// GoogleTest-style test output and sends it to a sink that you define.
/// See ``pw::unit_test::EventHandler`` for explanations of emitted events.
///
/// Example:
///
/// @code{.cpp}
///   #include "pw_unit_test/framework.h"
///   // pw_unit_test:light requires an event handler to be configured.
///   #include "pw_unit_test/simple_printing_event_handler.h"
///
///   void WriteString(const std::string_view& string, bool newline) {
///     printf("%s", string.data());
///     if (newline) {
///       printf("\n");
///     }
///   }
///
///   int main() {
///     // The following line has no effect with pw_unit_test_light, but makes
///     // this test compatible with upstream GoogleTest.
///     testing::InitGoogleTest();
///     // Since we are using pw_unit_test:light, set up an event handler.
///     pw::unit_test::SimplePrintingEventHandler handler(WriteString);
///     pw::unit_test::RegisterEventHandler(&handler);
///     return RUN_ALL_TESTS();
///   }
/// @endcode
///
/// Example output:
///
/// @code{.txt}
///   >>> Running MyTestSuite.TestCase1
///   [SUCCESS] 128 <= 129
///   [FAILURE] 'a' == 'b'
///     at ../path/to/my/file_test.cc:4831
///   <<< Test MyTestSuite.TestCase1 failed
/// @endcode
class SimplePrintingEventHandler : public GoogleTestStyleEventHandler {
 public:
  // Function for writing output as a string.
  using WriteFunction = void (*)(const std::string_view& string,
                                 bool append_newline);

  // Instantiates an event handler with a function to which to output results.
  // If verbose is set, information for successful tests is written as well as
  // failures.
  constexpr SimplePrintingEventHandler(WriteFunction write_function,
                                       bool verbose = false)
      : GoogleTestStyleEventHandler(verbose),
        write_(write_function),
        buffer_{} {}

 private:
  void WriteLine(const char* format, ...) override PW_PRINTF_FORMAT(2, 3);

  void Write(const char* content) override { write_(content, false); }

  WriteFunction write_;
  char buffer_[512];
};

}  // namespace pw::unit_test
