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

#ifdef __cplusplus

#include <cstdint>

namespace pw::cpu_exception::risc_v {

// This is dictated by this module, and shouldn't change often.
// Note that the order of entries in the following structs is very important
// (as the values are populated in assembly). The registers are typically stored
// onto the stack in a specific order by the exception entry
struct ExceptionRegisters {
  uint32_t ra;
  uint32_t sp;
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
  uint32_t fp;
  uint32_t s1;
  uint32_t a0;
  uint32_t a1;
  uint32_t a2;
  uint32_t a3;
  uint32_t a4;
  uint32_t a5;
  uint32_t a6;
  uint32_t a7;
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
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
};
static_assert(sizeof(ExceptionRegisters) == (sizeof(uint32_t) * 29),
              "There's unexpected padding.");

// This is dictated by this module, and shouldn't change often.
//
// NOTE: Memory mapped registers are NOT restored upon fault return!
struct ExtraRegisters {
  uint32_t mepc;
  uint32_t mcause;
  uint32_t mstatus;
  uint32_t mtval;
};
static_assert(sizeof(ExtraRegisters) == (sizeof(uint32_t) * 4),
              "There's unexpected padding.");

}  // namespace pw::cpu_exception::risc_v

// This is dictated by this module, and shouldn't change often.
struct pw_cpu_exception_State {
  pw::cpu_exception::risc_v::ExceptionRegisters base;
  pw::cpu_exception::risc_v::ExtraRegisters extended;
};

#else  // !__cplusplus

typedef struct pw_cpu_exception_State pw_cpu_exception_State;

#endif  // __cplusplus
