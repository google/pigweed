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

#include "pw_system/crash_handler.h"

#include "pw_assert_trap/message.h"
#include "pw_cpu_exception/handler.h"
#include "pw_log/log.h"
#include "pw_system/crash_snapshot.h"
#include "pw_system/device_handler.h"

namespace pw::system {

namespace {

CrashSnapshot crash_snapshot;

extern "C" void pw_system_exception_handler(pw_cpu_exception_State* state) {
  // TODO: b/354769112 - handle nested exceptions.
  PW_LOG_CRITICAL("CPU exception encountered!");

  auto assert_message = pw_assert_trap_get_message();
  crash_snapshot.Capture(*state, assert_message);

  device_handler::RebootSystem();
}

}  // namespace

void RegisterCrashHandler() {
  pw_cpu_exception_SetHandler(pw_system_exception_handler);
}

}  // namespace pw::system
