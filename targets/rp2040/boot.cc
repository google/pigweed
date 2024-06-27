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

#include <array>

#define PW_LOG_MODULE_NAME "pw_system"

#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_string/util.h"
#include "pw_system/init.h"
#include "task.h"

int main() {
  // PICO_SDK Inits
  stdio_init_all();
  setup_default_uart();
  // stdio_usb_init();
  PW_LOG_INFO("pw_system main");

  pw::system::Init();
  vTaskStartScheduler();
  PW_UNREACHABLE;
}
