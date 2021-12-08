// Copyright 2021 The Pigweed Authors
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

#include "pw_sys_io/sys_io.h"

#include <cinttypes>

#include "pw_status/status.h"
#include "stm32cube/stm32cube.h"

#define _CAT(A, B, C) A##B##C
#define CAT(A, B, C) _CAT(A, B, C)

#define USART_INSTANCE CAT(USART, USART_NUM, )
#define USART_GPIO_ALTERNATE_FUNC CAT(GPIO_AF7_USART, USART_NUM, )
#define USART_GPIO_PORT CAT(GPIO, USART_GPIO_PORT_CHAR, )
#define USART_GPIO_TX_PIN CAT(GPIO_PIN_, USART_TX_PIN_NUM, )
#define USART_GPIO_RX_PIN CAT(GPIO_PIN_, USART_RX_PIN_NUM, )

#define USART_GPIO_PORT_ENABLE \
  CAT(__HAL_RCC_GPIO, USART_GPIO_PORT_CHAR, _CLK_ENABLE)
#define USART_ENABLE CAT(__HAL_RCC_USART, USART_NUM, _CLK_ENABLE)

static UART_HandleTypeDef uart;

extern "C" void pw_sys_io_Init() {
  GPIO_InitTypeDef GPIO_InitStruct = {};

  USART_ENABLE();
  USART_GPIO_PORT_ENABLE();

  GPIO_InitStruct.Pin = USART_GPIO_TX_PIN | USART_GPIO_RX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = USART_GPIO_ALTERNATE_FUNC;
  HAL_GPIO_Init(USART_GPIO_PORT, &GPIO_InitStruct);

  uart.Instance = USART_INSTANCE;
  uart.Init.BaudRate = 115200;
  uart.Init.WordLength = UART_WORDLENGTH_8B;
  uart.Init.StopBits = UART_STOPBITS_1;
  uart.Init.Parity = UART_PARITY_NONE;
  uart.Init.Mode = UART_MODE_TX_RX;
  uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&uart);
}

// This whole implementation is very inefficient because it uses the synchronous
// polling UART API and only reads / writes 1 byte at a time.
namespace pw::sys_io {
Status ReadByte(std::byte* dest) {
  if (HAL_UART_Receive(
          &uart, reinterpret_cast<uint8_t*>(dest), 1, HAL_MAX_DELAY) !=
      HAL_OK) {
    return Status::ResourceExhausted();
  }
  return OkStatus();
}

Status TryReadByte(std::byte* dest) { return Status::Unimplemented(); }

Status WriteByte(std::byte b) {
  if (HAL_UART_Transmit(
          &uart, reinterpret_cast<uint8_t*>(&b), 1, HAL_MAX_DELAY) != HAL_OK) {
    return Status::ResourceExhausted();
  }
  return OkStatus();
}

// Writes a string using pw::sys_io, and add newline characters at the end.
StatusWithSize WriteLine(const std::string_view& s) {
  size_t chars_written = 0;
  StatusWithSize result = WriteBytes(std::as_bytes(std::span(s)));
  if (!result.ok()) {
    return result;
  }
  chars_written += result.size();

  // Write trailing newline.
  result = WriteBytes(std::as_bytes(std::span("\r\n", 2)));
  chars_written += result.size();

  return StatusWithSize(OkStatus(), chars_written);
}

}  // namespace pw::sys_io
