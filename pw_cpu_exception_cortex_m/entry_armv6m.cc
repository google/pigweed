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

#include <cstdint>
#include <cstring>

#include "pw_cpu_exception/entry.h"
#include "pw_cpu_exception/handler.h"
#include "pw_cpu_exception_cortex_m/cpu_state.h"
#include "pw_cpu_exception_cortex_m/util.h"
#include "pw_cpu_exception_cortex_m_private/cortex_m_constants.h"

namespace pw::cpu_exception::cortex_m {
namespace {

// Calculate the address of the CPU pushed context dependent on whether the
// MSP or PSP was in use at exception.
void* GetContextLocation(pw_cpu_exception_State& cpu_state) {
  if (ProcessStackActive(cpu_state)) {
    return reinterpret_cast<void*>(cpu_state.extended.psp);
  }

  // pw_cpu_exception_Entry() will always push pw_cpu_exception_State to the MSP
  // so offset past that to get to the CPU pushed context.
  return reinterpret_cast<void*>(cpu_state.extended.msp +
                                 sizeof(pw_cpu_exception_State));
}

// Copy the CPU pushed context on exception the into cpu_state.
//
// For more information see ARMv6-M Section B1.5.11, exceptions
// on exception entry.
void CloneExceptionRegistersFromPushedContext(
    pw_cpu_exception_State& cpu_state) {
  // On ARMv6-M, it's safe to always copy the pushed exception context because
  // if the context cannot be pushed onto the stack the cpu goes into lockup,
  // and there is no path forwards.
  void* context = GetContextLocation(cpu_state);
  std::memcpy(&cpu_state.base, context, sizeof(ExceptionRegisters));
}

// Restore the CPU pushed context on exception from the cpu_state.
//
// For more information see ARMv6-M Section B1.5.11, exceptions
// on exception entry.
void RestoreExceptionRegistersToPushedContext(
    pw_cpu_exception_State& cpu_state) {
  // On ARMv6-M, it's safe to always copy (and hence clone) the pushed exception
  // context because if the context cannot be pushed onto the stack the cpu goes
  // into lockup, and there is no path forwards.
  void* context = GetContextLocation(cpu_state);
  std::memcpy(context, &cpu_state.base, sizeof(ExceptionRegisters));
}

// Determines the size of the CPU-pushed context frame.
uint32_t CpuContextSize(const pw_cpu_exception_State& cpu_state) {
  uint32_t cpu_context_size = sizeof(ExceptionRegisters);
  if (cpu_state.base.psr & kPsrExtraStackAlignBit) {
    // Account for the extra 4-bytes the processor
    // added to keep the stack pointer 8-byte aligned
    cpu_context_size += 4;
  }

  return cpu_context_size;
}

// On exception entry, the Program Stack Pointer is patched to reflect the state
// at exception-time. On exception return, it is restored to the appropriate
// location. This calculates the delta that is used for these patch operations.
uint32_t CalculatePspDelta(const pw_cpu_exception_State& cpu_state) {
  // If CPU context was not pushed to program stack (because program stack
  // wasn't in use), the PSP doesn't need to be shifted.
  if (MainStackActive(cpu_state)) {
    return 0;
  }

  return CpuContextSize(cpu_state);
}

// On exception entry, the Main Stack Pointer is patched to reflect the state
// at exception-time. On exception return, it is restored to the appropriate
// location. This calculates the delta that is used for these patch operations.
uint32_t CalculateMspDelta(const pw_cpu_exception_State& cpu_state) {
  // pw_cpu_exception_State is always pushed to the MSP stack.
  uint32_t delta = sizeof(pw_cpu_exception_State);
  // If CPU context was pushed to main stack (because main stack
  // was in use), the MSP needs to be shifted.
  if (MainStackActive(cpu_state)) {
    delta += CpuContextSize(cpu_state);
  }
  return delta;
}

}  // namespace

extern "C" {

// Collect remaining CPU state (memory mapped registers), populate memory mapped
// registers, and call application exception handler.
PW_USED void pw_PackageAndHandleCpuException(
    pw_cpu_exception_State* cpu_state) {
  // Capture memory mapped registers.
  cpu_state->extended.icsr = cortex_m_icsr();
  cpu_state->extended.shcsr = cortex_m_shcsr();

  // The CPU will have automatically pushed state to PSP or MSP.
  // The values can be copied into in the pw_cpu_exception_State struct that is
  // passed to HandleCpuException(). The cpu_state passed to the handler is
  // ALWAYS stored on the main stack (MSP).
  CloneExceptionRegistersFromPushedContext(*cpu_state);

  // Patch captured stack pointers so they reflect the state at exception time.
  cpu_state->extended.msp += CalculateMspDelta(*cpu_state);
  cpu_state->extended.psp += CalculatePspDelta(*cpu_state);

  // Call application-level exception handler.
  pw_cpu_exception_HandleException(cpu_state);

  // Restore program stack pointer so exception return can restore state if
  // needed.
  // Note: The default behavior of NOT subtracting a delta from MSP is
  // intentional. This simplifies the assembly to pop the exception state
  // off the main stack on exception return (since MSP currently reflects
  // exception-time state).
  cpu_state->extended.psp -= CalculatePspDelta(*cpu_state);
  if (MainStackActive(*cpu_state)) {
    cpu_state->extended.msp -= CalculateMspDelta(*cpu_state);
  }

  RestoreExceptionRegistersToPushedContext(*cpu_state);
}

// Captures faulting CPU state on the main stack (MSP), then calls the exception
// handlers.
// This function should be called immediately after an exception.
PW_USED PW_NO_PROLOGUE void pw_cpu_exception_Entry() {
  asm volatile(
      // clang-format off

      // Enable unified syntax for Thumb and Thumb2.
      " .syntax unified                         \n"

      //
      // This code is logically very similar to the ARMv7-M+ exception entry,
      // except simpler due to ARMv6-M limitations.  Specifically the
      // pw_cpu_exception_State struct is always pushed to the stack.
      //
      // Regardless of where the PSP or MSP was used, always reserve stack space
      // for the pw_cpu_exception_State struct. Since we're
      // in exception handler mode, the main stack pointer is currently in use.
      " sub sp, sp, %[exception_state_size]     \n"

      // Store GPRs to stack.
      " str r4, [sp, #24]                       \n"  // ExtraRegisters.r4
      " str r5, [sp, #28]                       \n"  // ExtraRegisters.r5
      " str r6, [sp, #32]                       \n"  // ExtraRegisters.r6
      " str r7, [sp, #36]                       \n"  // ExtraRegisters.r7
      " mov r1, r8                              \n"
      " str r1, [sp, #40]                       \n"  // ExtraRegisters.r8
      " mov r1, r9                              \n"
      " str r1, [sp, #44]                       \n"  // ExtraRegisters.r9
      " mov r1, r10                             \n"
      " str r1, [sp, #48]                       \n"  // ExtraRegisters.r10
      " mov r1, r11                             \n"
      " str r1, [sp, #52]                       \n"  // ExtraRegisters.r11

      // Load special registers.
      " mov r1, lr                              \n"
      " mrs r2, msp                             \n"
      " mrs r3, psp                             \n"
      " mrs r4, control                         \n"

      // Store special registers to stack.
      " str r1, [sp, #8]                        \n" // ExtraRegisters.exc_return
      " str r2, [sp, #12]                       \n" // ExtraRegisters.msp
      " str r3, [sp, #16]                       \n" // ExtraRegisters.psp
      " str r4, [sp, #20]                       \n" // ExtraRegisters.control

      // Store in r4 a pointer to the beginning of where the special registers
      // start (offset 8 to skipping memory mapped registers), so they can be
      // restored later.
      " mov r4, sp                              \n"
      " adds r4, #8                             \n" // ExtraRegisters.exc_return

      // Restore captured_cpu_state pointer to r0. This makes adding more
      // memory mapped registers easier in the future since they're skipped in
      // this assembly.
      " mrs r0, msp                             \n"

      // Call intermediate handler that packages data.
      " ldr r3, =pw_PackageAndHandleCpuException\n"
      " blx r3                                  \n"

      // Restore state and exit exception handler.
      // Pointer to saved CPU state was stored in r4.
      " mov r0, r4                              \n"

      // Restore special registers.
      " ldm r0!, {r1-r4}                        \n"
      " mov lr, r1                              \n"
      " msr control, r4                         \n"

      // Restore low GPRs.
      " ldm r0!, {r4-r7}                        \n"
      // Restore high GPRs.
      " ldr r1, [r0]                            \n"
      " mov r8, r1                              \n"
      " adds r0, #4                             \n"
      " ldr r1, [r0]                            \n"
      " mov r9, r1                              \n"
      " adds r0, #4                             \n"
      " ldr r1, [r0]                            \n"
      " mov r10, r1                             \n"
      " adds r0, #4                             \n"
      " ldr r1, [r0]                            \n"
      " mov r11, r1                             \n"

      // Restore stack pointers.
      " msr msp, r2                             \n"
      " msr psp, r3                             \n"

      // Exit exception.
      " bx lr                                   \n"

      : /*output=*/
      : /*input=*/[exception_state_size]"i"(sizeof(pw_cpu_exception_State))
      // clang-format on
  );
}

}  // extern "C"
}  // namespace pw::cpu_exception::cortex_m
