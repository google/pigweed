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
#include "pw_stream_uart_mcuxpresso/interrupt_safe_writer.h"

#include "fsl_clock.h"

namespace pw::stream {

pw::Status InterruptSafeUartWriterMcuxpresso::Enable() {
  usart_config_t usart_config;
  USART_GetDefaultConfig(&usart_config);
  usart_config.baudRate_Bps = baudrate_;
  usart_config.enableRx = false;
  usart_config.enableTx = true;

  if (USART_Init(base(), &usart_config, CLOCK_GetFreq(clock_name_)) !=
      kStatus_Success) {
    return pw::Status::Internal();
  }

  return pw::OkStatus();
}

pw::Status InterruptSafeUartWriterMcuxpresso::DoWrite(pw::ConstByteSpan data) {
  const status_t hal_status = USART_WriteBlocking(
      base(), reinterpret_cast<const uint8_t*>(data.data()), data.size_bytes());
  return hal_status == kStatus_Success ? pw::OkStatus()
                                       : pw::Status::Internal();
}

}  // namespace pw::stream
