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
#pragma once

#include <cstdint>
#include <optional>

#include "pw_status/status.h"
#include "pw_stream/stream.h"

namespace pw::stream {

/// @module{pw_stream_uart_linux}

/// `pw::stream::NonSeekableReaderWriter` implementation for UARTs on Linux.
class UartStreamLinux : public NonSeekableReaderWriter {
 public:
  // Linux UART configuration structure.
  //
  // Properties are listed as optional - we can allow the user to leave the
  // default properties un-changed.
  struct Config {
    std::optional<uint32_t> baud_rate;
    std::optional<bool> flow_control;
  };

  constexpr UartStreamLinux() = default;

  // UartStream objects are moveable but not copyable.
  UartStreamLinux& operator=(UartStreamLinux&& other) {
    fd_ = other.fd_;
    other.fd_ = kInvalidFd;
    return *this;
  }
  UartStreamLinux(UartStreamLinux&& other) noexcept : fd_(other.fd_) {
    other.fd_ = kInvalidFd;
  }
  UartStreamLinux(const UartStreamLinux&) = delete;
  UartStreamLinux& operator=(const UartStreamLinux&) = delete;

  ~UartStreamLinux() override { Close(); }

  /// Open a UART device using the specified baud rate.
  ///
  /// @param[in] path Path to the TTY device.
  /// @param[in] baud_rate Baud rate to use for the device.
  ///
  /// @return @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The device was successfully opened and configured.
  ///
  ///    INVALID_ARGUMENT: An unsupported baud rate was supplied.
  ///
  ///    FAILED_PRECONDITION: A device was already open.
  ///
  ///    UNKNOWN: An error was returned by the operating system.
  ///
  /// @endrst
  Status Open(const char* path, uint32_t baud_rate) {
    return Open(path, {.baud_rate = baud_rate, .flow_control = std::nullopt});
  }

  /// Open a UART device using the specified Config struct.
  ///
  /// @param[in] path Path to the TTY device.
  /// @param[in] config UartStreamLinux configuration structure.
  ///
  /// @return @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The device was successfully opened and configured.
  ///
  ///    INVALID_ARGUMENT: Invalid config, for e.g. unsupported baud rate.
  ///
  ///    FAILED_PRECONDITION: A device was already open.
  ///
  ///    UNKNOWN: An error was returned by the operating system.
  ///
  /// @endrst
  Status Open(const char* path, Config config);

  void Close();

 private:
  static constexpr int kInvalidFd = -1;

  Status DoWrite(ConstByteSpan data) override;
  StatusWithSize DoRead(ByteSpan dest) override;

  int fd_ = kInvalidFd;
};

}  // namespace pw::stream
