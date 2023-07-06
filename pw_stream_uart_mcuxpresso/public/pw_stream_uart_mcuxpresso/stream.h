// Copyright 2023 The Pigweed Authors
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

#include "fsl_usart_freertos.h"
#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"

namespace pw::stream {

class UartStreamMcuxpresso : public NonSeekableReaderWriter {
 public:
  UartStreamMcuxpresso(USART_Type* base,
                       uint32_t baudrate,
                       usart_parity_mode_t parity,
                       usart_stop_bit_count_t stopbits,
                       ByteSpan buffer)
      : base_(base),
        config_{.base = base_,
                .srcclk = 0,
                .baudrate = baudrate,
                .parity = parity,
                .stopbits = stopbits,
                .buffer = reinterpret_cast<uint8_t*>(buffer.data()),
                .buffer_size = buffer.size()} {}

  ~UartStreamMcuxpresso();

  pw::Status Init(uint32_t srcclk);

 private:
  StatusWithSize DoRead(ByteSpan) override;
  Status DoWrite(ConstByteSpan) override;

  USART_Type* base_;
  struct rtos_usart_config config_;
  usart_rtos_handle_t handle_;
  usart_handle_t uart_handle_;
};

}  // namespace pw::stream
