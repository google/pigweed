// Copyright 2020 The Pigweed Authors
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

namespace pw::interrupt {

#include <cstdint>

#if defined(PW_INTERRUPT_CORTEX_M_ARMV7M) || \
    defined(PW_INTERRUPT_CORTEX_M_ARMV8M)
inline bool InInterruptContext() {
  // ARMv7M Reference manual section B1.4.2 describes how the Interrupt
  // Program Status Register (IPSR) is zero if there is no exception (interrupt)
  // being processed.
  uint32_t ipsr;
  asm volatile("MRS %0, ipsr" : "=r"(ipsr));
  return ipsr != 0;
}
#else
#error "Please select an architecture specific backend."
#endif

}  // namespace pw::interrupt
