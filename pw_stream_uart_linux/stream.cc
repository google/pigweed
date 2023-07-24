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

#include "pw_stream_uart_linux/stream.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cinttypes>

#include "pw_log/log.h"

namespace pw::stream {

Status UartStreamLinux::Open(const char* path, uint32_t baud_rate) {
  speed_t speed;

  // Extend switch statement as needed to support additional baud rates.
  switch (baud_rate) {
    case 115200:
      speed = B115200;
      break;
    default:
      PW_LOG_ERROR("Unsupported baud rate: %" PRIu32, baud_rate);
      return Status::InvalidArgument();
  }

  if (fd_ != kInvalidFd) {
    PW_LOG_ERROR("UART device already open");
    return Status::FailedPrecondition();
  }

  fd_ = open(path, O_RDWR);
  if (fd_ < 0) {
    PW_LOG_ERROR(
        "Failed to open UART device '%s', %s", path, std::strerror(errno));
    return Status::Unknown();
  }

  struct termios tty;
  int result = tcgetattr(fd_, &tty);
  if (result < 0) {
    PW_LOG_ERROR("Failed to get TTY attributes for '%s', %s",
                 path,
                 std::strerror(errno));
    return Status::Unknown();
  }

  cfmakeraw(&tty);
  result = cfsetspeed(&tty, speed);
  if (result < 0) {
    PW_LOG_ERROR(
        "Failed to set TTY speed for '%s', %s", path, std::strerror(errno));
    return Status::Unknown();
  }

  result = tcsetattr(fd_, TCSANOW, &tty);
  if (result < 0) {
    PW_LOG_ERROR("Failed to set TTY attributes for '%s', %s",
                 path,
                 std::strerror(errno));
    return Status::Unknown();
  }

  return OkStatus();
}

void UartStreamLinux::Close() {
  if (fd_ != kInvalidFd) {
    close(fd_);
    fd_ = kInvalidFd;
  }
}

Status UartStreamLinux::DoWrite(ConstByteSpan data) {
  const size_t size = data.size_bytes();
  size_t written = 0;
  while (written < size) {
    int bytes = write(fd_, &data[written], size - written);
    if (bytes < 0) {
      PW_LOG_ERROR("Failed to write to UART, %s", std::strerror(errno));
      return Status::Unknown();
    }
    written += bytes;
  }
  return OkStatus();
}

StatusWithSize UartStreamLinux::DoRead(ByteSpan dest) {
  int bytes = read(fd_, &dest[0], dest.size_bytes());
  if (bytes < 0) {
    PW_LOG_ERROR("Failed to read from UART, %s", std::strerror(errno));
    return StatusWithSize::Unknown();
  }
  return StatusWithSize(bytes);
}

}  // namespace pw::stream
