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

/// @defgroup FreeRTOS_application_functions
///
/// FreeRTOS requires the application to implement certain functions, depending
/// on its configuration.
///
/// If static allocation (`configSUPPORT_STATIC_ALLOCATION`) is enabled and
/// `configKERNEL_PROVIDED_STATIC_MEMORY` is disabled, FreeRTOS requires
/// applications to implement functions that provide static memory for the idle
/// task and timer task. See https://www.freertos.org/a00110.html for details.
///
/// Link against `"//third_party/freertos:support"` to include these function
/// implementations.
/// @{

#include <cstdint>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

extern "C" {

#if configSUPPORT_STATIC_ALLOCATION == 1 &&           \
    (!defined(configKERNEL_PROVIDED_STATIC_MEMORY) || \
     configKERNEL_PROVIDED_STATIC_MEMORY == 0)

#if ((tskKERNEL_VERSION_MAJOR == 11) && (tskKERNEL_VERSION_MINOR >= 1)) || \
    tskKERNEL_VERSION_MAJOR > 11
#define TASK_STACK_SIZE_TYPE configSTACK_DEPTH_TYPE
#else
#define TASK_STACK_SIZE_TYPE uint32_t
#endif  // tskKERNEL_VERSION_MAJOR >= 11

/// Allocates static memory for the idle task. Provides a
/// `configMINIMAL_STACK_SIZE` stack.
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t** ppxIdleTaskStackBuffer,
                                   TASK_STACK_SIZE_TYPE* pulIdleTaskStackSize) {
  static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
  static StaticTask_t idle_tcb;

  *ppxIdleTaskTCBBuffer = &idle_tcb;
  *ppxIdleTaskStackBuffer = idle_stack;
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if configUSE_TIMERS == 1

/// Allocates static memory for the timer task. Provides a
/// `configTIMER_TASK_STACK_DEPTH` stack.
void vApplicationGetTimerTaskMemory(
    StaticTask_t** ppxTimerTaskTCBBuffer,
    StackType_t** ppxTimerTaskStackBuffer,
    TASK_STACK_SIZE_TYPE* pulTimerTaskStackSize) {
  static StackType_t timer_stack[configTIMER_TASK_STACK_DEPTH];
  static StaticTask_t timer_tcb;

  *ppxTimerTaskTCBBuffer = &timer_tcb;
  *ppxTimerTaskStackBuffer = timer_stack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif  // configUSE_TIMERS == 1
#endif  // configSUPPORT_STATIC_ALLOCATION == 1

}  // extern "C"

/// @}
