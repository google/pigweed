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

#include "pw_cpu_exception_armv7m/cpu_state.h"

#include <cinttypes>
#include <cstdint>
#include <span>

#include "pw_cpu_exception/support.h"
#include "pw_log/log.h"
#include "pw_string/string_builder.h"

namespace pw::cpu_exception {

std::span<const uint8_t> RawFaultingCpuState(
    const pw_CpuExceptionState& cpu_state) {
  return std::span(reinterpret_cast<const uint8_t*>(&cpu_state),
                   sizeof(cpu_state));
}

// Using this function adds approximately 100 bytes to binary size.
void ToString(const pw_CpuExceptionState& cpu_state,
              const std::span<char>& dest) {
  StringBuilder builder(dest);
  const ArmV7mFaultRegisters& base = cpu_state.base;
  const ArmV7mExtraRegisters& extended = cpu_state.extended;

#define _PW_FORMAT_REGISTER(state_section, name) \
  builder.Format("%s=0x%08" PRIx32 "\n", #name, state_section.name)

  // Other registers.
  _PW_FORMAT_REGISTER(base, pc);
  _PW_FORMAT_REGISTER(base, lr);
  _PW_FORMAT_REGISTER(base, psr);
  _PW_FORMAT_REGISTER(extended, msp);
  _PW_FORMAT_REGISTER(extended, psp);
  _PW_FORMAT_REGISTER(extended, exc_return);
  _PW_FORMAT_REGISTER(extended, cfsr);
  _PW_FORMAT_REGISTER(extended, mmfar);
  _PW_FORMAT_REGISTER(extended, bfar);
  _PW_FORMAT_REGISTER(extended, icsr);
  _PW_FORMAT_REGISTER(extended, hfsr);
  _PW_FORMAT_REGISTER(extended, shcsr);
  _PW_FORMAT_REGISTER(extended, control);

  // General purpose registers.
  _PW_FORMAT_REGISTER(base, r0);
  _PW_FORMAT_REGISTER(base, r1);
  _PW_FORMAT_REGISTER(base, r2);
  _PW_FORMAT_REGISTER(base, r3);
  _PW_FORMAT_REGISTER(extended, r4);
  _PW_FORMAT_REGISTER(extended, r5);
  _PW_FORMAT_REGISTER(extended, r6);
  _PW_FORMAT_REGISTER(extended, r7);
  _PW_FORMAT_REGISTER(extended, r8);
  _PW_FORMAT_REGISTER(extended, r9);
  _PW_FORMAT_REGISTER(extended, r10);
  _PW_FORMAT_REGISTER(extended, r11);
  _PW_FORMAT_REGISTER(base, r12);

#undef _PW_FORMAT_REGISTER
}

// Using this function adds approximately 100 bytes to binary size.
void LogCpuState(const pw_CpuExceptionState& cpu_state) {
  const ArmV7mFaultRegisters& base = cpu_state.base;
  const ArmV7mExtraRegisters& extended = cpu_state.extended;

  PW_LOG_INFO("Captured CPU state:");

#define _PW_LOG_REGISTER(state_section, name) \
  PW_LOG_INFO("  %-10s 0x%08" PRIx32, #name, state_section.name)

  // Other registers.
  _PW_LOG_REGISTER(base, pc);
  _PW_LOG_REGISTER(base, lr);
  _PW_LOG_REGISTER(base, psr);
  _PW_LOG_REGISTER(extended, msp);
  _PW_LOG_REGISTER(extended, psp);
  _PW_LOG_REGISTER(extended, exc_return);
  _PW_LOG_REGISTER(extended, cfsr);
  _PW_LOG_REGISTER(extended, mmfar);
  _PW_LOG_REGISTER(extended, bfar);
  _PW_LOG_REGISTER(extended, icsr);
  _PW_LOG_REGISTER(extended, hfsr);
  _PW_LOG_REGISTER(extended, shcsr);
  _PW_LOG_REGISTER(extended, control);

  // General purpose registers.
  _PW_LOG_REGISTER(base, r0);
  _PW_LOG_REGISTER(base, r1);
  _PW_LOG_REGISTER(base, r2);
  _PW_LOG_REGISTER(base, r3);
  _PW_LOG_REGISTER(extended, r4);
  _PW_LOG_REGISTER(extended, r5);
  _PW_LOG_REGISTER(extended, r6);
  _PW_LOG_REGISTER(extended, r7);
  _PW_LOG_REGISTER(extended, r8);
  _PW_LOG_REGISTER(extended, r9);
  _PW_LOG_REGISTER(extended, r10);
  _PW_LOG_REGISTER(extended, r11);
  _PW_LOG_REGISTER(base, r12);

#undef _PW_LOG_REGISTER
}

}  // namespace pw::cpu_exception
