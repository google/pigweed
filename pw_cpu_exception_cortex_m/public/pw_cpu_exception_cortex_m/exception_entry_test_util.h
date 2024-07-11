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

#include <stddef.h>

#include <cstdint>
#include <type_traits>

namespace pw::cpu_exception::cortex_m {

// Constants and utilities that are common to ARMv6, ARMv7 and ARMv8.

// CCR flags. (ARMv6-M Section B3.2.8)
constexpr uint32_t kUnalignedTrapEnableMask = 0x1u << 3;

// Magic pattern to help identify if the exception handler's
// pw_cpu_exception_State pointer was pointing to captured CPU state that was
// pushed onto the stack.
constexpr uint32_t kMagicPattern = 0xDEADBEEF;

void InstallVectorTableEntries(uint32_t* exception_entry_addr);

}  // namespace pw::cpu_exception::cortex_m
