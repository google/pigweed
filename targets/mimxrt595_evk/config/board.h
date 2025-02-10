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

#include "fsl_common.h"
#include "pw_preprocessor/util.h"

#define BOARD_DEBUG_UART_TYPE kSerialPort_Uart
#define BOARD_DEBUG_UART_BASEADDR (uint32_t)USART0
#define BOARD_DEBUG_UART_INSTANCE 0U
#define BOARD_DEBUG_UART_CLK_FREQ CLOCK_GetFlexcommClkFreq(0)
#define BOARD_DEBUG_UART_FRG_CLK \
  (&(const clock_frg_clk_config_t){0U, kCLOCK_FrgPllDiv, 255U, 0U})
#define BOARD_DEBUG_UART_CLK_ATTACH kFRG_to_FLEXCOMM0
#define BOARD_UART_IRQ_HANDLER FLEXCOMM0_IRQHandler
#define BOARD_UART_IRQ FLEXCOMM0_IRQn
#ifndef BOARD_DEBUG_UART_BAUDRATE
#define BOARD_DEBUG_UART_BAUDRATE 115200
#endif

PW_EXTERN_C_START

void BOARD_InitDebugConsole(void);
AT_QUICKACCESS_SECTION_CODE(void BOARD_SetFlexspiClock(FLEXSPI_Type* base,
                                                       uint32_t src,
                                                       uint32_t divider));
AT_QUICKACCESS_SECTION_CODE(void BOARD_DeinitFlash(FLEXSPI_Type* base));
AT_QUICKACCESS_SECTION_CODE(void BOARD_InitFlash(FLEXSPI_Type* base));

PW_EXTERN_C_END
