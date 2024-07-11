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

#include "pw_cpu_exception_cortex_m/exception_entry_test_util.h"

#include <cstring>

#include "pw_cpu_exception_cortex_m_private/cortex_m_constants.h"

namespace pw::cpu_exception::cortex_m {

volatile uint32_t& cortex_m_vtor =
    *reinterpret_cast<volatile uint32_t*>(0xE000ED08u);

// In-memory interrupt service routine vector table.
using InterruptVectorTable = std::aligned_storage_t<512, 512>;
InterruptVectorTable ram_vector_table;

// Begin a critical section that must not be interrupted.
// This function disables interrupts to prevent any sort of context switch until
// the critical section ends. This is done by setting PRIMASK to 1 using the cps
// instruction.
//
// Returns the state of PRIMASK before it was disabled.
inline uint32_t BeginCriticalSection() {
  uint32_t previous_state;
  asm volatile(
      " mrs %[previous_state], primask              \n"
      " cpsid i                                     \n"
      // clang-format off
      : /*output=*/[previous_state]"=r"(previous_state)
      : /*input=*/
      : /*clobbers=*/"memory"
      // clang-format on
  );
  return previous_state;
}

// Ends a critical section.
// Restore previous previous state produced by BeginCriticalSection().
// Note: This does not always re-enable interrupts.
inline void EndCriticalSection(uint32_t previous_state) {
  asm volatile(
      // clang-format off
      "msr primask, %0"
      : /*output=*/
      : /*input=*/"r"(previous_state)
      : /*clobbers=*/"memory"
      // clang-format on
  );
}

void InstallVectorTableEntries(uint32_t* exception_entry_addr) {
  uint32_t prev_state = BeginCriticalSection();
  // If vector table is installed already, this is done.
  if (cortex_m_vtor == reinterpret_cast<uint32_t>(&ram_vector_table)) {
    EndCriticalSection(prev_state);
    return;
  }
  // Copy table to new location since it's not guaranteed that we can write to
  // the original one.
  std::memcpy(&ram_vector_table,
              reinterpret_cast<uint32_t*>(cortex_m_vtor),
              sizeof(ram_vector_table));

  // Override exception handling vector table entries.
  uint32_t** interrupts = reinterpret_cast<uint32_t**>(&ram_vector_table);
  interrupts[kHardFaultIsrNum] = exception_entry_addr;
  // On v6-M, only HardFault is supported
#if !_PW_ARCH_ARM_V6M
  interrupts[kMemFaultIsrNum] = exception_entry_addr;
  interrupts[kBusFaultIsrNum] = exception_entry_addr;
  interrupts[kUsageFaultIsrNum] = exception_entry_addr;
#endif

  // Update Vector Table Offset Register (VTOR) to point to new vector table.
  cortex_m_vtor = reinterpret_cast<uint32_t>(&ram_vector_table);
  EndCriticalSection(prev_state);
}

}  // namespace pw::cpu_exception::cortex_m
