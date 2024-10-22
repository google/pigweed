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

#define PW_LOG_MODULE_NAME "pw_system"

#include "FreeRTOS.h"
#include "hardware/exception.h"
#include "pico/stdlib.h"
#include "pw_cpu_exception/entry.h"
#include "pw_log/log.h"
#include "pw_preprocessor/arch.h"
#include "pw_system/init.h"
#include "task.h"

#if !_PW_ARCH_ARM_V6M
#include "RP2350.h"
#endif  // !_PW_ARCH_ARM_V6M

int main() {
  // PICO_SDK Inits
  stdio_init_all();
  setup_default_uart();
  // stdio_usb_init();

  // Install the CPU exception handler.
  exception_set_exclusive_handler(HARDFAULT_EXCEPTION, pw_cpu_exception_Entry);
  // On RP2040 (arm6m), only HardFault is supported
#if !_PW_ARCH_ARM_V6M
  // TODO: b/373723963 - The pico sdk exception_number enum doesn't currently
  // have values for MemManage, BusFault or UsageFault, so cast the values for
  // now until pico sdk has been updated. Enable the MemManage handler
  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
  exception_set_exclusive_handler(static_cast<exception_number>(4),
                                  pw_cpu_exception_Entry);
  // Enable the BusFault handler
  SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
  exception_set_exclusive_handler(static_cast<exception_number>(5),
                                  pw_cpu_exception_Entry);
  // Enable the UsageFault handler
  SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
  exception_set_exclusive_handler(static_cast<exception_number>(6),
                                  pw_cpu_exception_Entry);
#endif  // !_PW_ARCH_ARM_V6M

  PW_LOG_INFO("pw_system main");

  pw::system::Init();
  vTaskStartScheduler();
  PW_UNREACHABLE;
}
