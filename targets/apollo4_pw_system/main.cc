// Copyright 2023 The Pigweed Authors
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

#define PW_LOG_MODULE_NAME "pw_system"

#include "FreeRTOS.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_string/util.h"
#include "pw_system/init.h"
#include "task.h"

// System core clock value definition, usually provided by the CMSIS package.
uint32_t SystemCoreClock = 96'000'000ul;

int main() {
  pw::system::Init();
  vTaskStartScheduler();

  PW_UNREACHABLE;
}
