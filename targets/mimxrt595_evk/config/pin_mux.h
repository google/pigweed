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

#define IOPCTL_PIO_ANAMUX_DI 0x00u
#define IOPCTL_PIO_FULLDRIVE_DI 0x00u
#define IOPCTL_PIO_FUNC1 0x01u
#define IOPCTL_PIO_INBUF_DI 0x00u
#define IOPCTL_PIO_INBUF_EN 0x40u
#define IOPCTL_PIO_INV_DI 0x00u
#define IOPCTL_PIO_PSEDRAIN_DI 0x00u
#define IOPCTL_PIO_PULLDOWN_EN 0x00u
#define IOPCTL_PIO_PULLUP_EN 0x20u
#define IOPCTL_PIO_PUPD_DI 0x00u
#define IOPCTL_PIO_PUPD_EN 0x10u
#define IOPCTL_PIO_SLEW_RATE_NORMAL 0x00u

PW_EXTERN_C void BOARD_InitBootPins();
