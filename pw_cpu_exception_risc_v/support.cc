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

#include "pw_cpu_exception/support.h"

#include <cinttypes>
#include <cstdint>

#include "pw_cpu_exception_risc_v/cpu_state.h"
#include "pw_cpu_exception_risc_v_private/config.h"
#include "pw_log/log.h"

namespace pw::cpu_exception {

// Using this function adds approximately 100 bytes to binary size.
void LogCpuState(const pw_cpu_exception_State& cpu_state) {
  const risc_v::ExceptionRegisters& base = cpu_state.base;
  const risc_v::ExtraRegisters& extended = cpu_state.extended;
  PW_LOG_INFO("All captured CPU registers:");

#define _PW_LOG_REGISTER(state_section, name) \
  PW_LOG_INFO("  %-10s 0x%08" PRIx32, #name, state_section.name)

  _PW_LOG_REGISTER(extended, mepc);
  _PW_LOG_REGISTER(extended, mcause);
  _PW_LOG_REGISTER(extended, mtval);
  _PW_LOG_REGISTER(extended, mstatus);

  // General purpose registers.
  _PW_LOG_REGISTER(base, ra);
  _PW_LOG_REGISTER(base, sp);
  _PW_LOG_REGISTER(base, t0);
  _PW_LOG_REGISTER(base, t1);
  _PW_LOG_REGISTER(base, t2);
  _PW_LOG_REGISTER(base, fp);
  _PW_LOG_REGISTER(base, s1);
  _PW_LOG_REGISTER(base, a0);
  _PW_LOG_REGISTER(base, a1);
  _PW_LOG_REGISTER(base, a2);
  _PW_LOG_REGISTER(base, a3);
  _PW_LOG_REGISTER(base, a4);
  _PW_LOG_REGISTER(base, a5);
  _PW_LOG_REGISTER(base, a6);
  _PW_LOG_REGISTER(base, a7);
  _PW_LOG_REGISTER(base, s2);
  _PW_LOG_REGISTER(base, s3);
  _PW_LOG_REGISTER(base, s4);
  _PW_LOG_REGISTER(base, s5);
  _PW_LOG_REGISTER(base, s6);
  _PW_LOG_REGISTER(base, s7);
  _PW_LOG_REGISTER(base, s8);
  _PW_LOG_REGISTER(base, s9);
  _PW_LOG_REGISTER(base, s10);
  _PW_LOG_REGISTER(base, s11);
  _PW_LOG_REGISTER(base, t3);
  _PW_LOG_REGISTER(base, t4);
  _PW_LOG_REGISTER(base, t5);
  _PW_LOG_REGISTER(base, t6);

#undef _PW_LOG_REGISTER
}

}  // namespace pw::cpu_exception
