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

#include "pw_preprocessor/util.h"

#if defined(_PW_ASSERT_TRAP_DISABLE_TRAP_FOR_TESTING) && \
    _PW_ASSERT_TRAP_DISABLE_TRAP_FOR_TESTING
#define _PW_ASSERT_TRAP_EPILOG() pw_assert_trap_interrupt_unlock();
#else
#define _PW_ASSERT_TRAP_EPILOG() __builtin_trap();
#endif  // defined(_PW_ASSERT_TRAP_DISABLE_TRAP_FOR_TESTING) &&
        // _PW_ASSERT_TRAP_DISABLE_TRAP_FOR_TESTING

PW_EXTERN_C_START

void pw_assert_trap_interrupt_lock(void);
void pw_assert_trap_interrupt_unlock(void);

// TODO: https://pwbug.dev/351904146 - tokenize these handlers.
void pw_assert_trap_HandleAssertFailure(const char* file_name,
                                        int line_number,
                                        const char* function_name);

void pw_assert_trap_HandleCheckFailure(const char* file_name,
                                       int line_number,
                                       const char* function_name,
                                       const char* format,
                                       ...);

PW_EXTERN_C_END
