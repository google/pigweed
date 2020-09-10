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
#pragma once

#include <cstdint>

#include "pw_preprocessor/compiler.h"

namespace pw::cpu_exception {

// This is dictated by ARMv7-M architecture. Do not change.
PW_PACKED(struct) ArmV7mFaultRegisters {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;   // Link register.
  uint32_t pc;   // Program counter.
  uint32_t psr;  // Program status register.
};

// This is dictated by ARMv7-M architecture. Do not change.
PW_PACKED(struct) ArmV7mFaultRegistersFpu {
  uint32_t s0;
  uint32_t s1;
  uint32_t s2;
  uint32_t s3;
  uint32_t s4;
  uint32_t s5;
  uint32_t s6;
  uint32_t s7;
  uint32_t s8;
  uint32_t s9;
  uint32_t s10;
  uint32_t s11;
  uint32_t s12;
  uint32_t s13;
  uint32_t s14;
  uint32_t s15;
  uint32_t fpscr;
  uint32_t reserved;
};

// Bit in the PSR that indicates CPU added an extra word on the stack to
// align it during context save for an exception.
inline constexpr uint32_t kPsrExtraStackAlignBit = (1 << 9);

// This is dictated by this module, and shouldn't change often.
// Note that the order of entries in this struct is very important (as the
// values are populated in assembly).
//
// NOTE: Memory mapped registers are NOT restored upon fault return!
PW_PACKED(struct) ArmV7mExtraRegisters {
  // Memory mapped registers.
  uint32_t cfsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t icsr;
  uint32_t hfsr;
  uint32_t shcsr;
  // Special registers.
  uint32_t exc_return;
  uint32_t msp;
  uint32_t psp;
  uint32_t control;
  // General purpose registers.
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
};

}  // namespace pw::cpu_exception

PW_PACKED(struct) pw_CpuExceptionState {
  pw::cpu_exception::ArmV7mExtraRegisters extended;
  pw::cpu_exception::ArmV7mFaultRegisters base;
  // TODO(amontanez): FPU registers may or may not be here as well. Make the
  // availability of the FPU registers a compile-time configuration when FPU
  // register support is added.
};
