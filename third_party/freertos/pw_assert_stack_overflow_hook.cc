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
#include "pw_string/util.h"
#include "task.h"

#if configCHECK_FOR_STACK_OVERFLOW != 0

/// @ingroup FreeRTOS_application_functions
///
/// If `configCHECK_FOR_STACK_OVERFLOW` is enabled, FreeRTOS requires
/// applications to implement `vApplicationStackOverflowHook`, which is called
/// when a stack overflow is detected. This implementation invokes
/// @c_macro{PW_CRASH} with the task name.
extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char* pcTaskName) {
  // Copy the task name to a buffer in case it is corrupted.
  static char temp_thread_name_buffer[configMAX_TASK_NAME_LEN];
  // TODO: https://pwbug.dev/357139112 - Review the IgnoreError call.
  pw::string::Copy(pcTaskName, temp_thread_name_buffer).IgnoreError();
  PW_CRASH("Stack overflow for task %s", temp_thread_name_buffer);
}

#endif  // configCHECK_FOR_STACK_OVERFLOW != 0
