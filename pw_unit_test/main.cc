// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#include <cstdio>

#include "pw_unit_test/framework.h"
#include "pw_unit_test/simple_printing_event_handler.h"

// Default main function for the pw_unit_test library. Prints all test results
// to stdout using a SimplePrintingEventHandler.
int main() {
  pw::unit_test::SimplePrintingEventHandler handler(
      [](const char* s) { return std::printf("%s", s); });
  pw::unit_test::RegisterEventHandler(&handler);
  return RUN_ALL_TESTS();
}
