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

#include "pw_cpu_exception/handler.h"

namespace pw::cpu_exception {

static void (*exception_handler)(pw_CpuExceptionState*) =
    &pw_CpuExceptionDefaultHandler;

extern "C" void pw_CpuExceptionSetHandler(
    void (*handler)(pw_CpuExceptionState*)) {
  exception_handler = handler;
}

// Revert the exception handler to point to pw_CpuExceptionDefaultHandler().
extern "C" void pw_CpuExceptionRestoreDefaultHandler() {
  exception_handler = &pw_CpuExceptionDefaultHandler;
}

extern "C" void pw_HandleCpuException(void* cpu_state) {
  exception_handler(reinterpret_cast<pw_CpuExceptionState*>(cpu_state));
}

}  // namespace pw::cpu_exception