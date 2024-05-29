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
#pragma once

#include "pw_cpu_exception_cortex_m/cpu_state.h"

// This is a fake backend header used to compile crash_test.
#define PW_CPU_EXCEPTION_CORTEX_M_HANDLE_CRASH(cpu_state, fmt, ...) \
  CaptureCrashAnalysisForTest(cpu_state, fmt, __VA_ARGS__)

namespace pw::cpu_exception::cortex_m {

void CaptureCrashAnalysisForTest(const pw_cpu_exception_State& cpu_stat,
                                 const char* fmt,
                                 ...);

}  // namespace pw::cpu_exception::cortex_m
