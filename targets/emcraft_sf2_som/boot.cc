// Copyright 2022 The Pigweed Authors
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

#include "pw_boot/boot.h"

#include <array>

#include "FreeRTOS.h"
#include "pw_boot_cortex_m/boot.h"
#include "pw_malloc/malloc.h"
#include "pw_preprocessor/compiler.h"
#include "pw_string/util.h"
#include "pw_sys_io_emcraft_sf2/init.h"
#include "pw_system/init.h"
#include "task.h"

namespace {

std::array<StackType_t, configMINIMAL_STACK_SIZE> freertos_idle_stack;
StaticTask_t freertos_idle_tcb;

std::array<StackType_t, configTIMER_TASK_STACK_DEPTH> freertos_timer_stack;
StaticTask_t freertos_timer_tcb;

std::array<char, configMAX_TASK_NAME_LEN> temp_thread_name_buffer;

}  // namespace

// Functions needed when configGENERATE_RUN_TIME_STATS is on.
extern "C" void configureTimerForRunTimeStats(void) {}
extern "C" unsigned long getRunTimeCounterValue(void) { return 10 /* FIXME */; }
// uwTick is an uint32_t incremented each Systick interrupt 1ms. uwTick is used
// to execute HAL_Delay function.

// Required for configCHECK_FOR_STACK_OVERFLOW.
extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char* pcTaskName) {
  pw::string::Copy(pcTaskName, temp_thread_name_buffer);
  PW_CRASH("Stack OVF for task %s", temp_thread_name_buffer.data());
}

// Required for configUSE_TIMERS.
extern "C" void vApplicationGetTimerTaskMemory(
    StaticTask_t** ppxIdleTaskTCBBuffer,
    StackType_t** ppxIdleTaskStackBuffer,
    uint32_t* pulIdleTaskStackSize) {
  *ppxIdleTaskTCBBuffer = &freertos_idle_tcb;
  *ppxIdleTaskStackBuffer = freertos_idle_stack.data();
  *pulIdleTaskStackSize = freertos_idle_stack.size();
}

extern "C" void vApplicationGetIdleTaskMemory(
    StaticTask_t** ppxIdleTaskTCBBuffer,
    StackType_t** ppxIdleTaskStackBuffer,
    uint32_t* pulIdleTaskStackSize) {
  *ppxIdleTaskTCBBuffer = &freertos_timer_tcb;
  *ppxIdleTaskStackBuffer = freertos_timer_stack.data();
  *pulIdleTaskStackSize = freertos_timer_stack.size();
}

extern "C" void pw_boot_PreStaticMemoryInit() {}

extern "C" void pw_boot_PreStaticConstructorInit() {
  // TODO(skeys) add "#if no_bootLoader" and the functions needed for init.

#if PW_MALLOC_ACTIVE
  pw_MallocInit(&pw_boot_heap_low_addr, &pw_boot_heap_high_addr);
#endif  // PW_MALLOC_ACTIVE
  pw_sys_io_Init();
}

// TODO(amontanez): pw_boot_PreMainInit() should get renamed to
// pw_boot_FinalizeBoot or similar when main() is removed.
extern "C" void pw_boot_PreMainInit() {
  pw::system::Init();
  vTaskStartScheduler();
  PW_UNREACHABLE;
}

// This `main()` stub prevents another main function from being linked since
// this target deliberately doesn't run `main()`.
extern "C" int main() {}

extern "C" PW_NO_RETURN void pw_boot_PostMain() {
  // In case main() returns, just sit here until the device is reset.
  while (true) {
  }
  PW_UNREACHABLE;
}
