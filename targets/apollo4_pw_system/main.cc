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

#define PW_LOG_MODULE_NAME "pw_system"

#include "FreeRTOS.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_string/util.h"
#include "pw_system/init.h"
#include "task.h"

// System core clock value definition, usually provided by the CMSIS package.
uint32_t SystemCoreClock = 96'000'000ul;

namespace {

#if configCHECK_FOR_STACK_OVERFLOW != 0
std::array<char, configMAX_TASK_NAME_LEN> temp_thread_name_buffer;
#endif  // configCHECK_FOR_STACK_OVERFLOW

#if configUSE_TIMERS == 1
std::array<StackType_t, configTIMER_TASK_STACK_DEPTH> freertos_timer_stack;
StaticTask_t freertos_timer_tcb;
#endif  // configUSE_TIMERS == 1

std::array<StackType_t, configMINIMAL_STACK_SIZE> freertos_idle_stack;
StaticTask_t freertos_idle_tcb;
}  // namespace

#if configCHECK_FOR_STACK_OVERFLOW != 0
PW_EXTERN_C void vApplicationStackOverflowHook(TaskHandle_t, char* pcTaskName) {
  pw::string::Copy(pcTaskName, temp_thread_name_buffer);
  PW_CRASH("Stack OVF for task %s", temp_thread_name_buffer.data());
}
#endif  // configCHECK_FOR_STACK_OVERFLOW

#if configUSE_TIMERS == 1
PW_EXTERN_C void vApplicationGetTimerTaskMemory(
    StaticTask_t** ppxTimerTaskTCBBuffer,
    StackType_t** ppxTimerTaskStackBuffer,
    uint32_t* pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &freertos_timer_tcb;
  *ppxTimerTaskStackBuffer = freertos_timer_stack.data();
  *pulTimerTaskStackSize = freertos_timer_stack.size();
}
#endif  // configUSE_TIMERS == 1

PW_EXTERN_C void vApplicationGetIdleTaskMemory(
    StaticTask_t** ppxIdleTaskTCBBuffer,
    StackType_t** ppxIdleTaskStackBuffer,
    uint32_t* pulIdleTaskStackSize) {
  *ppxIdleTaskTCBBuffer = &freertos_idle_tcb;
  *ppxIdleTaskStackBuffer = freertos_idle_stack.data();
  *pulIdleTaskStackSize = freertos_idle_stack.size();
}

int main() {
  pw::system::Init();
  vTaskStartScheduler();

  PW_UNREACHABLE;
}
