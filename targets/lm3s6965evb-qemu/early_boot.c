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

#include "pw_boot_armv7m/boot.h"
#include "pw_sys_io_baremetal_lm3s6965evb/init.h"

void pw_boot_PreStaticMemoryInit() {
  // Force RCC to be at default at boot.
  const uint32_t kRccDefault = 0x078E3AD1U;
  volatile uint32_t* rcc = (volatile uint32_t*)0x400FE070U;
  *rcc = kRccDefault;
  const uint32_t kRcc2Default = 0x07802810U;
  volatile uint32_t* rcc2 = (volatile uint32_t*)0x400FE070U;
  *rcc2 = kRcc2Default;
}

void pw_boot_PreStaticConstructorInit() {}

void pw_boot_PreMainInit() { pw_sys_io_Init(); }
