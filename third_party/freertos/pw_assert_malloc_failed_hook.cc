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

#include "FreeRTOS.h"
#include "pw_assert/check.h"
#include "task.h"

#if configUSE_MALLOC_FAILED_HOOK != 0

/// @submodule{third_party,freertos}

/// If `configUSE_MALLOC_FAILED_HOOK` is enabled, FreeRTOS requires
/// applications to implement `vApplicationMallocFailedHook`, which is called
/// when a heap allocation fails. This implementation invokes
/// @c_macro{PW_CRASH} with the remaining heap size.
extern "C" void vApplicationMallocFailedHook() {
  PW_CRASH("Malloc failed to allocate, remaining heap size: %d",
           xPortGetFreeHeapSize());
}

/// @}

#endif  // configUSE_MALLOC_FAILED_HOOK != 0
