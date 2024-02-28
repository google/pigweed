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

/*
 * Copyright 2018 - 2022 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pw_stream_uart_mcuxpresso/dma_stream.h"

#include "pw_preprocessor/util.h"

namespace pw::stream {

// Initialize the USART and DMA channels based on the configuration
// specified during object creation.
Status UartDmaStreamMcuxpresso::Init(uint32_t srcclk) {
  if (srcclk == 0) {
    return Status::InvalidArgument();
  }
  if (config_.usart_base == nullptr) {
    return Status::InvalidArgument();
  }
  if (config_.baud_rate == 0) {
    return Status::InvalidArgument();
  }
  if (config_.dma_base == nullptr) {
    return Status::InvalidArgument();
  }
  return OkStatus();
}

StatusWithSize UartDmaStreamMcuxpresso::DoRead(ByteSpan data) {
  size_t length = data.size();
  return StatusWithSize(length);
}

Status UartDmaStreamMcuxpresso::DoWrite(ConstByteSpan data) {
  return OkStatus();
}

}  // namespace pw::stream
