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

#include "pw_cpu_exception_cortex_m/crash.h"

#include <cinttypes>

#include "pw_cpu_exception_cortex_m/cpu_state.h"
#include "pw_cpu_exception_cortex_m/util.h"
#include "pw_cpu_exception_cortex_m_private/config.h"
#include "pw_cpu_exception_cortex_m_private/cortex_m_constants.h"
#include "pw_preprocessor/arch.h"

#if PW_CPU_EXCEPTION_CORTEX_M_CRASH_ANALYSIS_INCLUDE_PC_LR
#define _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING \
  " PC=0x%08" PRIx32 " LR=0x%08" PRIx32
#define _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS \
  cpu_state.base.pc, cpu_state.base.lr,
#else
#define _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING ""
#define _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
#endif  // PW_CPU_EXCEPTION_CORTEX_M_CRASH_ANALYSIS_INCLUDE_PC_LR

namespace pw::cpu_exception::cortex_m {

void AnalyzeCpuStateAndCrash(const pw_cpu_exception_State& cpu_state,
                             const char* optional_thread_name) {
  const char* thread_name =
      optional_thread_name == nullptr ? "?" : optional_thread_name;
  const bool is_nested_fault =
      cpu_state.extended.hfsr & (kHfsrForcedMask | kHfsrDebugEvtMask);
  const uint32_t active_faults =
      cpu_state.extended.cfsr &
      (kCfsrMemAllErrorsMask | kCfsrBusAllErrorsMask | kCfsrUsageAllErrorsMask);
  // The expression (n & (n - 1)) is 0 if n is a power of 2, i.e. n has
  // only 1 bit (fault flag) set. So if it is != 0 there are multiple faults.
  const bool has_multiple_faults = (active_faults & (active_faults - 1)) != 0;

  // This provides a high-level assessment of the cause of the exception.
  // These conditionals are ordered by priority to ensure the most critical
  // issues are highlighted first. These are not mutually exclusive; a bus fault
  // could occur during the handling of a MPU violation, causing a nested fault.

  // Debug monitor exception.
  if ((cpu_state.extended.icsr & kIcsrVectactiveMask) == kDebugMonIsrNum) {
    PW_CPU_EXCEPTION_CORTEX_M_CRASH(
        cpu_state,
        "Debug Monitor exception triggered."
        " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
        " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
        thread_name,
        // clang-format off
        _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
        cpu_state.extended.cfsr,
        // clang-format on
        is_nested_fault,
        has_multiple_faults);
    return;
  }
#if _PW_ARCH_ARM_V8M_MAINLINE || _PW_ARCH_ARM_V8_1M_MAINLINE
  if (cpu_state.extended.cfsr & kCfsrStkofMask) {
    if (ProcessStackActive(cpu_state)) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "PSP stack overflow."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " PSP=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          cpu_state.extended.psp,
          is_nested_fault,
          has_multiple_faults);
    } else {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "MSP stack overflow."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " MSP=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          cpu_state.extended.msp,
          is_nested_fault,
          has_multiple_faults);
    }
    return;
  }
#endif  // _PW_ARCH_ARM_V8M_MAINLINE || _PW_ARCH_ARM_V8_1M_MAINLINE

  // Memory management fault.
  if (cpu_state.extended.cfsr & kCfsrMemFaultMask) {
    const bool is_mmfar_valid = cpu_state.extended.cfsr & kCfsrMmarvalidMask;
#if PW_CPU_EXCEPTION_CORTEX_M_CRASH_EXTENDED_CPU_ANALYSIS
    if (cpu_state.extended.cfsr & kCfsrIaccviolMask) {
      // The PC value stacked for the exception return points to the faulting
      // instruction.
      // The processor does not write the fault address to the MMFAR.
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "IACCVIOL: MPU violation on instruction fetch."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          //clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrDaccviolMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "DACCVIOL: MPU violation on memory read/write."
          " Thread=%s " _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " ValidMmfar=%d MMFAR=0x%08" PRIx32
          " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_mmfar_valid,
          cpu_state.extended.mmfar,
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrMunstkerrMask) {
      // The processor does not write the fault address to the MMFAR.
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "MUNSTKERR: MPU violation on exception return."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrMstkerrMask) {
      // The processor does not write the fault address to the MMFAR.
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "MSTKERR: MPU violation on exception entry."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrMlsperrMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "MLSPERR: MPU violation on lazy FPU state preservation."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " ValidMmfar=%d MMFAR=0x%08" PRIx32
          " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_mmfar_valid,
          cpu_state.extended.mmfar,
          is_nested_fault,
          has_multiple_faults);
      return;
    }
#endif  // PW_CPU_EXCEPTION_CORTEX_M_CRASH_EXTENDED_CPU_ANALYSIS

    PW_CPU_EXCEPTION_CORTEX_M_CRASH(
        cpu_state,
        "MPU fault. Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
        " CFSR=0x%08" PRIx32 " ValidMmfar=%d MMFAR=0x%08" PRIx32
        " Nested=%d Multiple=%d",
        thread_name,
        // clang-format off
        _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
        cpu_state.extended.cfsr,
        // clang-format on
        is_mmfar_valid,
        cpu_state.extended.mmfar,
        is_nested_fault,
        has_multiple_faults);
    return;
  }

  // Bus fault.
  if (cpu_state.extended.cfsr & kCfsrBusFaultMask) {
    const bool is_bfar_valid = cpu_state.extended.cfsr & kCfsrBfarvalidMask;
#if PW_CPU_EXCEPTION_CORTEX_M_CRASH_EXTENDED_CPU_ANALYSIS
    if (cpu_state.extended.cfsr & kCfsrIbuserrMask) {
      // The processor does not write the fault address to the BFAR.
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "IBUSERR: Bus fault on instruction fetch."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrPreciserrMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "PRECISERR: Precise bus fault."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " ValidBfar=%d BFAR=0x%08" PRIx32
          " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_bfar_valid,
          cpu_state.extended.bfar,
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrImpreciserrMask) {
      // The processor does not write the fault address to the BFAR.
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "IMPRECISERR: Imprecise bus fault."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrUnstkerrMask) {
      // The processor does not write the fault address to the BFAR.
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "UNSTKERR: Derived bus fault on exception context save."
          " Thread=%s " _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrStkerrMask) {
      // The processor does not write the fault address to the BFAR.
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "STKERR: Derived bus fault on exception context restore."
          " Thread=%s " _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrLsperrMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "LSPERR: Derived bus fault on lazy FPU state preservation."
          " Thread=%s " _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " ValidBfar=%d BFAR=0x%08" PRIx32
          " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_bfar_valid,
          cpu_state.extended.bfar,
          is_nested_fault,
          has_multiple_faults);
      return;
    }
#endif  // PW_CPU_EXCEPTION_CORTEX_M_CRASH_EXTENDED_CPU_ANALYSIS

    PW_CPU_EXCEPTION_CORTEX_M_CRASH(
        cpu_state,
        "Bus Fault. Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
        " CFSR=0x%08" PRIx32 " ValidBfar=%d BFAR=0x%08" PRIx32
        " Nested=%d Multiple=%d",
        thread_name,
        // clang-format off
        _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
        cpu_state.extended.cfsr,
        // clang-format on
        is_bfar_valid,
        cpu_state.extended.bfar,
        is_nested_fault,
        has_multiple_faults);
    return;
  }

  // Usage fault.
  if (cpu_state.extended.cfsr & kCfsrUsageFaultMask) {
#if PW_CPU_EXCEPTION_CORTEX_M_CRASH_EXTENDED_CPU_ANALYSIS
    if (cpu_state.extended.cfsr & kCfsrUndefinstrMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "UNDEFINSTR: Encountered invalid instruction."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrInvstateMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "INVSTATE: Attempted instruction with invalid EPSR value."
          " Thread=%s " _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrInvpcMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "INVPC: Invalid program counter."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrNocpMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "NOCP: Coprocessor disabled or not present."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrUnalignedMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "UNALIGNED: Unaligned memory access."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
    if (cpu_state.extended.cfsr & kCfsrDivbyzeroMask) {
      PW_CPU_EXCEPTION_CORTEX_M_CRASH(
          cpu_state,
          "DIVBYZERO: Division by zero."
          " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
          " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
          thread_name,
          // clang-format off
          _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
          cpu_state.extended.cfsr,
          // clang-format on
          is_nested_fault,
          has_multiple_faults);
      return;
    }
#endif  // PW_CPU_EXCEPTION_CORTEX_M_CRASH_EXTENDED_CPU_ANALYSIS
    PW_CPU_EXCEPTION_CORTEX_M_CRASH(
        cpu_state,
        "Usage Fault. Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
        " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d ",
        thread_name,
        // clang-format off
        _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
        cpu_state.extended.cfsr,
        // clang-format on
        is_nested_fault,
        has_multiple_faults);
    return;
  }
  if ((cpu_state.extended.icsr & kIcsrVectactiveMask) == kNmiIsrNum) {
    PW_CPU_EXCEPTION_CORTEX_M_CRASH(
        cpu_state,
        "Non-Maskable Interrupt triggered."
        " Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
        " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
        thread_name,
        // clang-format off
        _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
        cpu_state.extended.cfsr,
        // clang-format on
        is_nested_fault,
        has_multiple_faults);
    return;
  }

  PW_CPU_EXCEPTION_CORTEX_M_CRASH(
      cpu_state,
      "Unknown fault. Thread=%s" _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
      " CFSR=0x%08" PRIx32 " Nested=%d Multiple=%d",
      thread_name,
      // clang-format off
      _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
      cpu_state.extended.cfsr,
      // clang-format on
      is_nested_fault,
      has_multiple_faults);
  return;
}

}  // namespace pw::cpu_exception::cortex_m

#undef _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_STRING
#undef _PW_CPU_EXCEPTION_CORTEX_M_PC_LR_ARGS
