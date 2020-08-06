// Copyright 2020 The Pigweed Authors
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

#include <Arduino.h>

#include <span>
#include <string_view>

#include "pw_sys_io/sys_io.h"
#include "pw_sys_io_arduino/init.h"
#include "pw_unit_test/framework.h"
#include "pw_unit_test/simple_printing_event_handler.h"

void loop() {}

void setup() {
  pw_sys_io_Init();

  pw::unit_test::SimplePrintingEventHandler handler(
      [](const std::string_view& s, bool append_newline) {
        if (append_newline) {
          pw::sys_io::WriteLine(s);
        } else {
          pw::sys_io::WriteBytes(std::as_bytes(std::span(s)));
        }
      });

  pw::unit_test::RegisterEventHandler(&handler);
  RUN_ALL_TESTS();
  // TODO(tonymd) Is there a better way to signal the end of a test?
  // Debug: write a character to signal end of test run
  Serial.write(23);
}
