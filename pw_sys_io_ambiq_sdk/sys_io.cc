// Copyright 2020 The Pigweed Authors
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

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "pw_preprocessor/compiler.h"

namespace {
void* hal_uart_handle{};
}  // namespace

PW_EXTERN_C void pw_sys_io_Init() {
  // Use baud rate of 115200 (8N1).
  static constexpr am_hal_uart_config_t kUartConfig = {
      .ui32BaudRate = 115200,
      .eDataBits = AM_HAL_UART_DATA_BITS_8,
      .eParity = AM_HAL_UART_PARITY_NONE,
      .eStopBits = AM_HAL_UART_ONE_STOP_BIT,
      .eFlowControl = AM_HAL_UART_FLOW_CTRL_NONE,
      .eTXFifoLevel = AM_HAL_UART_FIFO_LEVEL_16,
      .eRXFifoLevel = AM_HAL_UART_FIFO_LEVEL_16,
  };

  // Initialize the UART peripheral.
  am_hal_uart_initialize(AM_BSP_UART_PRINT_INST, &hal_uart_handle);

  // Change the power state of the UART peripheral.
  am_hal_uart_power_control(hal_uart_handle, AM_HAL_SYSCTRL_WAKE, false);

  // Configure UART (baudrate etc.).
  am_hal_uart_configure(hal_uart_handle, &kUartConfig);

  // Enable the UART TX and RX GPIO's.
  am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_TX, g_AM_BSP_GPIO_COM_UART_TX);
  am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_RX, g_AM_BSP_GPIO_COM_UART_RX);
}

namespace pw::sys_io {

// Wait for a byte to read on UART0. This blocks until a byte is read. This is
// extremely inefficient as it requires the target to burn CPU cycles polling to
// see if a byte is ready yet.
Status ReadByte(std::byte* dest) {
  while (true) {
    if (TryReadByte(dest).ok()) {
      return OkStatus();
    }
  }
}

Status TryReadByte(std::byte* dest) {
  am_hal_uart_transfer_t transaction{};
  uint32_t bytes_read{};

  // Configure UART transaction for the read operation.
  transaction.eType = AM_HAL_UART_BLOCKING_READ;
  transaction.pui8Data = reinterpret_cast<uint8_t*>(dest);
  transaction.ui32NumBytes = 1;
  transaction.ui32TimeoutMs = AM_HAL_UART_WAIT_FOREVER;
  transaction.pui32BytesTransferred = &bytes_read;

  // Do read data over UART.
  if (am_hal_uart_transfer(hal_uart_handle, &transaction) !=
      AM_HAL_STATUS_SUCCESS) {
    return Status::ResourceExhausted();
  }

  if (bytes_read != 1u) {
    return Status::DataLoss();
  }

  return OkStatus();
}

Status WriteByte(std::byte b) {
  am_hal_uart_transfer_t transaction{};
  uint32_t chars_written{};

  // Configure UART transaction for the write operation.
  transaction.eType = AM_HAL_UART_BLOCKING_WRITE;
  transaction.pui8Data =
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&b));
  transaction.ui32NumBytes = 1;
  transaction.ui32TimeoutMs = AM_HAL_UART_WAIT_FOREVER;
  transaction.pui32BytesTransferred = &chars_written;

  // Do write data over UART.
  if (am_hal_uart_transfer(hal_uart_handle, &transaction) !=
      AM_HAL_STATUS_SUCCESS) {
    return Status::ResourceExhausted();
  }

  if (chars_written != 1) {
    return Status::DataLoss();
  }

  return OkStatus();
}

// Writes a string using pw::sys_io, and add newline characters at the end.
StatusWithSize WriteLine(std::string_view s) {
  StatusWithSize result = WriteBytes(as_bytes(span(s)));
  if (!result.ok()) {
    return result;
  }

  size_t chars_written = result.size();
  if (chars_written != s.size()) {
    return StatusWithSize::DataLoss(chars_written);
  }

  // Write trailing newline.
  result = WriteBytes(as_bytes(span("\r\n", 2)));
  chars_written += result.size();

  if (result.size() != 2) {
    return StatusWithSize::DataLoss(chars_written);
  }

  return StatusWithSize(chars_written);
}

}  // namespace pw::sys_io
