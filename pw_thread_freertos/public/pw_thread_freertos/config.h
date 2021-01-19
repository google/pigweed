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
// Configuration macros for the tokenizer module.
#pragma once

#include "FreeRTOS.h"
#include "task.h"

// Whether thread joining is enabled. By default this is disabled.
// When enabled this adds a StaticEventGroup_t & EventGroupHandle_t to
// every pw::thread::Thread's context.
#ifndef PW_THREAD_FREERTOS_CONFIG_JOINING_ENABLED
#define PW_THREAD_FREERTOS_CONFIG_JOINING_ENABLED 0
#endif  // PW_THREAD_FREERTOS_CONFIG_JOINING_ENABLED
#define PW_THREAD_JOINING_ENABLED PW_THREAD_FREERTOS_CONFIG_JOINING_ENABLED

// Whether dynamic allocation for thread contexts is enabled. By default this
// matches the FreeRTOS configuration on whether dynamic allocations are
// enabled. Note that static contexts _must_ be provided if dynamic allocations
// are disabled.
#ifndef PW_THREAD_FREERTOS_CONFIG_DYNAMIC_ALLOCATION_ENABLED
#if configSUPPORT_DYNAMIC_ALLOCATION == 1
#define PW_THREAD_FREERTOS_CONFIG_DYNAMIC_ALLOCATION_ENABLED 1
#else
#define PW_THREAD_FREERTOS_CONFIG_DYNAMIC_ALLOCATION_ENABLED 0
#endif  // configSUPPORT_DYNAMIC_ALLOCATION
#endif  // PW_THREAD_FREERTOS_CONFIG_DYNAMIC_ALLOCATION_ENABLED

// The default stack size in words. By default this uses the minimal FreeRTOS
// stack size.
#ifndef PW_THREAD_FREERTOS_CONFIG_DEFAULT_STACK_SIZE_WORDS
#define PW_THREAD_FREERTOS_CONFIG_DEFAULT_STACK_SIZE_WORDS \
  configMINIMAL_STACK_SIZE
#endif  // PW_THREAD_FREERTOS_CONFIG_DEFAULT_STACK_SIZE_WORDS

// The default stack size in words. By default this uses the minimal FreeRTOS
// priority level above the idle priority.
#ifndef PW_THREAD_FREERTOS_CONFIG_DEFAULT_PRIORITY
#define PW_THREAD_FREERTOS_CONFIG_DEFAULT_PRIORITY tskIDLE_PRIORITY + 1
#endif  // PW_THREAD_FREERTOS_CONFIG_DEFAULT_PRIORITY

namespace pw::thread::freertos::config {

inline constexpr size_t kMinimumStackSizeWords = configMINIMAL_STACK_SIZE;
inline constexpr size_t kDefaultStackSizeWords =
    PW_THREAD_FREERTOS_CONFIG_DEFAULT_STACK_SIZE_WORDS;
inline constexpr UBaseType_t kDefaultPriority =
    PW_THREAD_FREERTOS_CONFIG_DEFAULT_PRIORITY;

}  // namespace pw::thread::freertos::config
