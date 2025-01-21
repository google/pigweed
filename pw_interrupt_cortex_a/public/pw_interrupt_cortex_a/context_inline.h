// Copyright 2025 The Pigweed Authors
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

namespace pw::interrupt {

// This module is not preprocessor protected like pw_interrupt_cortex_m because
// pw_preprocessor/arch.h does not have macros for Arm A-profile architectures.
// And it is not straightforward to add them because of the "cortex_m"
// cc_library. A more ground up refactor of pw_preprocessor is needed. For now,
// leave the responsibility to the user this backend only for Arm A-profile
// architectures.

#if ((__ARM_ARCH_PROFILE == 'A') && (__ARM_ARCH == 8) && \
     (__ARM_ARCH_ISA_A64 == 1))
inline bool InInterruptContext() {
  // Arm A-profile Architecture Registers describes if the Interrupt Status
  // Register (ISR_EL1) is zero if there is no exception (interrupt) being
  // processed.
  uint32_t isr_el1;
  asm volatile("MRS %0, ISR_EL1" : "=r"(isr_el1));
  return isr_el1 != 0;
}
#else
#error "This module is only intended for 64-bit ARMv8-A processors."
#endif  // ((__ARM_ARCH_PROFILE == 'A') && (__ARM_ARCH == 8) &&
        // (__ARM_ARCH_ISA_A64 == 1))

}  // namespace pw::interrupt
