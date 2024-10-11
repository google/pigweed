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

#include "pw_stream/stream.h"
#include "pw_uart/uart.h"

namespace pw::uart {

/// UartStream implements the pw::stream::NonSeekableReaderWriter
/// interface on top of a Uart device.
class UartStream final : public stream::NonSeekableReaderWriter {
 public:
  /// Constructs a UartStream for a Uart device.
  UartStream(Uart& uart) : uart_(uart) {}

 private:
  Uart& uart_;

  // NonSeekableReaderWriter impl.
  StatusWithSize DoRead(ByteSpan destination) override {
    return uart_.ReadAtLeast(destination, 1);
  }

  Status DoWrite(ConstByteSpan data) override { return uart_.Write(data); }
};

}  // namespace pw::uart
