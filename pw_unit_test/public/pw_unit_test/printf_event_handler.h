// Copyright 2022 The Pigweed Authors
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

#include <cstdarg>
#include <cstdio>

#include "pw_unit_test/googletest_style_event_handler.h"

namespace pw {
namespace unit_test {

/// Predefined event handler implementation that produces human-readable
/// GoogleTest-style test output and logs it via ``std::printf``. See
/// ``pw::unit_test::EventHandler`` for explanations of emitted events.
class PrintfEventHandler final : public GoogleTestStyleEventHandler {
 public:
  constexpr PrintfEventHandler(bool verbose = false)
      : GoogleTestStyleEventHandler(verbose) {}

 private:
  void Write(const char* content) override { std::printf("%s", content); }

  void WriteLine(const char* format, ...) override {
    std::va_list args;

    va_start(args, format);
    std::vprintf(format, args);
    std::printf("\n");
    va_end(args);
  }
};

}  // namespace unit_test
}  // namespace pw
