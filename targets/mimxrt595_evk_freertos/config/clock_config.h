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

#define BOARD_SYSOSC_SETTLING_US 220U
#define BOARD_XTAL32K_CLK_HZ 32768U
#define BOARD_XTAL_SYS_CLK_HZ 24000000U
#define BOARD_BOOTCLOCKRUN_CORE_CLOCK 198000000U

PW_EXTERN_C void BOARD_InitBootClocks(void);
