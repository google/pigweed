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

#include "pw_cpu_exception/entry.h"

#include <cstdint>
#include <cstring>

#include "pw_cpu_exception/handler.h"
#include "pw_cpu_exception_armv7m/cpu_state.h"
#include "pw_preprocessor/compiler.h"

namespace pw::cpu_exception {
namespace {

// CMSIS/Cortex-M/ARMv7 related constants.
// These values are from the ARMv7-M Architecture Reference Manual DDI 0403E.b.
// https://static.docs.arm.com/ddi0403/e/DDI0403E_B_armv7m_arm.pdf

// Masks for individual bits of CFSR. (ARMv7-M Section B3.2.15)
constexpr uint32_t kMemFaultStart = 0x1u;
constexpr uint32_t kMStkErrMask = kMemFaultStart << 4;
constexpr uint32_t kBusFaultStart = 0x1u << 8;
constexpr uint32_t kStkErrMask = kBusFaultStart << 4;

// Bit masks for an exception return value. (ARMv7-M Section B1.5.8)
constexpr uint32_t kExcReturnStackMask = (0x1u << 2);
constexpr uint32_t kExcReturnBasicFrameMask = (0x1u << 4);

// Memory mapped registers. (ARMv7-M Section B3.2.2, Table B3-4)
volatile uint32_t& arm_v7m_cfsr =
    *reinterpret_cast<volatile uint32_t*>(0xE000ED28u);
volatile uint32_t& arm_v7m_mmfar =
    *reinterpret_cast<volatile uint32_t*>(0xE000ED34u);
volatile uint32_t& arm_v7m_bfar =
    *reinterpret_cast<volatile uint32_t*>(0xE000ED38u);
volatile uint32_t& arm_v7m_icsr =
    *reinterpret_cast<volatile uint32_t*>(0xE000ED04u);
volatile uint32_t& arm_v7m_hfsr =
    *reinterpret_cast<volatile uint32_t*>(0xE000ED2Cu);
volatile uint32_t& arm_v7m_shcsr =
    *reinterpret_cast<volatile uint32_t*>(0xE000ED24u);

// If the CPU fails to capture some registers, the captured struct members will
// be populated with this value. The only registers that this value should be
// loaded into are pc, lr, and psr when the CPU fails to push an exception
// context frame.
//
// 0xFFFFFFFF is an illegal lr value, which is why it was selected for this
// purpose. pc and psr values of 0xFFFFFFFF are dubious too, so this constant
// is clear enough at expressing that the registers weren't properly captured.
constexpr uint32_t kInvalidRegisterValue = 0xFFFFFFFF;

// Checks exc_return in the captured CPU state to determine which stack pointer
// was in use prior to entering the exception handler.
bool PspWasActive(const pw_CpuExceptionState& cpu_state) {
  return cpu_state.extended.exc_return & kExcReturnStackMask;
}

// Checks exc_return to determine if FPU state was pushed to the stack in
// addition to the base CPU context frame.
bool FpuStateWasPushed(const pw_CpuExceptionState& cpu_state) {
  return !(cpu_state.extended.exc_return & kExcReturnBasicFrameMask);
}

// If the CPU successfully pushed context on exception, copy it into cpu_state.
//
// For more information see (See ARMv7-M Section B1.5.11, derived exceptions
// on exception entry).
void CloneBaseRegistersFromPsp(pw_CpuExceptionState* cpu_state) {
  // If CPU succeeded in pushing context to PSP, copy it to the MSP.
  if (!(cpu_state->extended.cfsr & kStkErrMask) &&
      !(cpu_state->extended.cfsr & kMStkErrMask)) {
    // TODO(amontanez): {r0-r3,r12} are captured in pw_CpuExceptionEntry(),
    //                  so this only really needs to copy pc, lr, and psr. Could
    //                  (possibly) improve speed, but would add marginally more
    //                  complexity.
    std::memcpy(&cpu_state->base,
                reinterpret_cast<void*>(cpu_state->extended.psp),
                sizeof(ArmV7mFaultRegisters));
  } else {
    // If CPU context wasn't pushed to stack on exception entry, we can't
    // recover psr, lr, and pc from exception-time. Make these values clearly
    // invalid.
    cpu_state->base.lr = kInvalidRegisterValue;
    cpu_state->base.pc = kInvalidRegisterValue;
    cpu_state->base.psr = kInvalidRegisterValue;
  }
}

// If the CPU successfully pushed context on exception, restore it from
// cpu_state. Otherwise, don't attempt to restore state.
//
// For more information see (See ARMv7-M Section B1.5.11, derived exceptions
// on exception entry).
void RestoreBaseRegistersToPsp(pw_CpuExceptionState* cpu_state) {
  // If CPU succeeded in pushing context to PSP on exception entry, restore the
  // contents of cpu_state to the CPU-pushed register frame so the CPU can
  // continue. Otherwise, don't attempt as we'll likely end up in an escalated
  // hard fault.
  if (!(cpu_state->extended.cfsr & kStkErrMask) &&
      !(cpu_state->extended.cfsr & kMStkErrMask)) {
    std::memcpy(reinterpret_cast<void*>(cpu_state->extended.psp),
                &cpu_state->base,
                sizeof(ArmV7mFaultRegisters));
  }
}

// Determines the size of the CPU-pushed context frame.
uint32_t CpuContextSize(const pw_CpuExceptionState& cpu_state) {
  uint32_t cpu_context_size = sizeof(ArmV7mFaultRegisters);
  if (FpuStateWasPushed(cpu_state)) {
    cpu_context_size += sizeof(ArmV7mFaultRegistersFpu);
  }
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
uint32_t CalculatePspDelta(const pw_CpuExceptionState& cpu_state) {
  // If CPU context was not pushed to program stack (because program stack
  // wasn't in use, or an error occurred when pushing context), the PSP doesn't
  // need to be shifted.
  if (!PspWasActive(cpu_state) || (cpu_state.extended.cfsr & kStkErrMask) ||
      (cpu_state.extended.cfsr & kMStkErrMask)) {
    return 0;
  }

  return CpuContextSize(cpu_state);
}

// On exception entry, the Main Stack Pointer is patched to reflect the state
// at exception-time. On exception return, it is restored to the appropriate
// location. This calculates the delta that is used for these patch operations.
uint32_t CalculateMspDelta(const pw_CpuExceptionState& cpu_state) {
  if (PspWasActive(cpu_state)) {
    // TODO(amontanez): Since FPU state isn't captured at this time, we ignore
    //                  it when patching MSP. To add FPU capture support,
    //                  delete this if block as CpuContextSize() will include
    //                  FPU context size in the calculation.
    return sizeof(ArmV7mFaultRegisters) + sizeof(ArmV7mExtraRegisters);
  }

  return CpuContextSize(cpu_state) + sizeof(ArmV7mExtraRegisters);
}

}  // namespace

extern "C" {

// Collect remaining CPU state (memory mapped registers), populate memory mapped
// registers, and call application exception handler.
PW_USED void pw_PackageAndHandleCpuException(pw_CpuExceptionState* cpu_state) {
  // Capture memory mapped registers.
  cpu_state->extended.cfsr = arm_v7m_cfsr;
  cpu_state->extended.mmfar = arm_v7m_mmfar;
  cpu_state->extended.bfar = arm_v7m_bfar;
  cpu_state->extended.icsr = arm_v7m_icsr;
  cpu_state->extended.hfsr = arm_v7m_hfsr;
  cpu_state->extended.shcsr = arm_v7m_shcsr;

  // CPU may have automatically pushed state to the program stack. If it did,
  // the values can be copied into in the pw_CpuExceptionState struct that is
  // passed to HandleCpuException(). The cpu_state passed to the handler is
  // ALWAYS stored on the main stack (MSP).
  if (PspWasActive(*cpu_state)) {
    CloneBaseRegistersFromPsp(cpu_state);
    // If PSP wasn't active, this delta is 0.
    cpu_state->extended.psp += CalculatePspDelta(*cpu_state);
  }

  // Patch captured stack pointers so they reflect the state at exception time.
  cpu_state->extended.msp += CalculateMspDelta(*cpu_state);

  // Call application-level exception handler.
  pw_HandleCpuException(cpu_state);

  // Restore program stack pointer so exception return can restore state if
  // needed.
  // Note: The default behavior of NOT subtracting a delta from MSP is
  // intentional. This simplifies the assembly to pop the exception state
  // off the main stack on exception return (since MSP currently reflects
  // exception-time state).
  cpu_state->extended.psp -= CalculatePspDelta(*cpu_state);

  // If PSP was active and the CPU pushed a context frame, we must copy the
  // potentially modified state from cpu_state back to the PSP so the CPU can
  // resume execution with the modified values.
  if (PspWasActive(*cpu_state)) {
    // In this case, there's no need to touch the MSP as it's at the location
    // before we entering the exception (effectively popping the state initially
    // pushed to the main stack).
    RestoreBaseRegistersToPsp(cpu_state);
  } else {
    // Since we're restoring context from MSP, we DO need to adjust MSP to point
    // to CPU-pushed context frame so it can be properly restored.
    // No need to adjust PSP since nothing was pushed to program stack.
    cpu_state->extended.msp -= CpuContextSize(*cpu_state);
  }
}

// Captures faulting CPU state on the main stack (MSP), then calls the exception
// handlers.
// This function should be called immediately after an exception.
void pw_CpuExceptionEntry(void) {
  asm volatile(
      // If PSP was in use at the time of exception, it's possible the CPU
      // wasn't able to push CPU state. To be safe, this first captures scratch
      // registers before moving forward.
      //
      // Stack flag is bit index 2 (0x4) of exc_return value stored in lr. When
      // this bit is set, the Process Stack Pointer (PSP) was in use. Otherwise,
      // the Main Stack Pointer (MSP) was in use. (See ARMv7-M Section B1.5.8
      // for more details)
      // The following block of assembly is equivalent to:
      //   if (lr & (1 << 2)) {
      //     msp -= sizeof(ArmV7mFaultRegisters);
      //     ArmV7mFaultRegisters* state = (ArmV7mFaultRegisters*) msp;
      //     state->r0 = r0;
      //     state->r1 = r1;
      //     state->r2 = r2;
      //     state->r3 = r3;
      //     state->r12 = r12;
      //   }
      //
      " tst lr, #(1 << 2)                                     \n"
      " itt ne                                                \n"
      " subne sp, sp, %[base_state_size]                      \n"
      " stmne sp, {r0-r3, r12}                                \n"

      // Reserve stack space for additional registers. Since we're in exception
      // handler mode, the main stack pointer is currently in use.
      // r0 will temporarily store the end of captured_cpu_state to simplify
      // assembly for copying additional registers.
      " mrs r0, msp                                           \n"
      " sub sp, sp, %[extra_state_size]                       \n"

      // Store GPRs to stack.
      " stmdb r0!, {r4-r11}                                   \n"

      // Load special registers.
      " mov r1, lr                                            \n"
      " mrs r2, msp                                           \n"
      " mrs r3, psp                                           \n"
      " mrs r4, control                                       \n"

      // Store special registers to stack.
      " stmdb r0!, {r1-r4}                                    \n"

      // Store a pointer to the beginning of special registers in r4 so they can
      // be restored later.
      " mov r4, r0                                            \n"

      // Restore captured_cpu_state pointer to r0. This makes adding more
      // memory mapped registers easier in the future since they're skipped in
      // this assembly.
      " mrs r0, msp                                           \n"

      // Call intermediate handler that packages data.
      " ldr r3, =pw_PackageAndHandleCpuException              \n"
      " blx r3                                                \n"

      // Restore state and exit exception handler.
      // Pointer to saved CPU state was stored in r4.
      " mov r0, r4                                            \n"

      // Restore special registers.
      " ldm r0!, {r1-r4}                                      \n"
      " mov lr, r1                                            \n"
      " msr control, r4                                       \n"

      // Restore GPRs.
      " ldm r0, {r4-r11}                                      \n"

      // Restore stack pointers.
      " msr msp, r2                                           \n"
      " msr psp, r3                                           \n"

      // Exit exception.
      " bx lr                                                 \n"
      // clang-format off
      : /*output=*/
      : /*input=*/[base_state_size]"i"(sizeof(ArmV7mFaultRegisters)),
                  [extra_state_size]"i"(sizeof(ArmV7mExtraRegisters))
      // clang-format on
  );
}

}  // extern "C"
}  // namespace pw::cpu_exception
