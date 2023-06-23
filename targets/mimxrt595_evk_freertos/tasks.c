// Copyright 2023 The Pigweed Authors
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

// This file provides implementations for idle and timer task memory when
// configSUPPORT_STATIC_ALLOCATION is set to 1.

void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t** ppxIdleTaskStackBuffer,
                                   uint32_t* pulIdleTaskStackSize) {
  static StaticTask_t tcb;
  static StackType_t stack[configMINIMAL_STACK_SIZE];

  *ppxIdleTaskTCBBuffer = &tcb;
  *ppxIdleTaskStackBuffer = stack;
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t** ppxTimerTaskStackBuffer,
                                    uint32_t* pulTimerTaskStackSize) {
  static StaticTask_t tcb;
  static StackType_t stack[configTIMER_TASK_STACK_DEPTH];

  *ppxTimerTaskTCBBuffer = &tcb;
  *ppxTimerTaskStackBuffer = stack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
