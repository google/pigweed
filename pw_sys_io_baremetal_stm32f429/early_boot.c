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

#include <inttypes.h>

#include "pw_boot_armv7m/boot.h"

void pw_PreStaticConstructorInit() {
  // TODO(pwbug/17): Optionally enable Replace when Pigweed config system is
  // added.
#if PW_ARMV7M_ENABLE_FPU
  // Enable FPU if built using hardware FPU instructions.
  // CPCAR mask that enables FPU. (ARMv7-M Section B3.2.20)
  const uint32_t kFpuEnableMask = (0xFu << 20);

  // Memory mapped register to enable FPU. (ARMv7-M Section B3.2.2, Table B3-4)
  volatile uint32_t* arm_v7m_cpacr = (volatile uint32_t*)0xE000ED88u;

  *arm_v7m_cpacr |= kFpuEnableMask;
#endif  // PW_ARMV7M_ENABLE_FPU
}