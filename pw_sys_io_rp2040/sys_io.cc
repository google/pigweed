// Copyright 2022 The Pigweed Authors
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

#include <stdio.h>

#include <cinttypes>

#include "pico/stdlib.h"
#include "pw_status/status.h"
#include "pw_sync/thread_notification.h"

namespace {

pw::sync::ThreadNotification chars_available_signal;

void chars_available_callback([[maybe_unused]] void* arg) {
  chars_available_signal.release();
}

void LazyInitSysIo() {
  static bool initialized = false;
  if (!initialized) {
    stdio_init_all();
    stdio_set_chars_available_callback(chars_available_callback, nullptr);
    initialized = true;
  }
}

// Spin until host connects.
void WaitForConnect() {
  // In order to stop this sleep polling, we could register a shared IRQ handler
  // for the USB interrupt and block on a signal from that.
  while (!stdio_usb_connected()) {
    sleep_ms(50);
  }
}

}  // namespace

// This whole implementation is very inefficient because it only reads / writes
// 1 byte at a time. It also does lazy initialization checks with every byte.
namespace pw::sys_io {

Status ReadByte(std::byte* dest) {
  LazyInitSysIo();
  WaitForConnect();

  while (true) {
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT) {
      *dest = static_cast<std::byte>(c);
      return OkStatus();
    }

    // Wait for signal from the chars_available_callback().
    chars_available_signal.acquire();
  }
}

Status TryReadByte(std::byte* dest) {
  LazyInitSysIo();
  int c = getchar_timeout_us(0);
  if (c == PICO_ERROR_TIMEOUT) {
    return Status::DeadlineExceeded();
  }
  *dest = static_cast<std::byte>(c);
  return OkStatus();
}

Status WriteByte(std::byte b) {
  // The return value of this is just the character sent.
  LazyInitSysIo();
  putchar_raw(static_cast<int>(b));
  return OkStatus();
}

// Writes a string using pw::sys_io, and add newline characters at the end.
StatusWithSize WriteLine(const std::string_view& s) {
  size_t chars_written = 0;
  StatusWithSize result = WriteBytes(as_bytes(span(s)));
  if (!result.ok()) {
    return result;
  }
  chars_written += result.size();

  // Write trailing newline.
  result = WriteBytes(as_bytes(span("\r\n", 2)));
  chars_written += result.size();

  return StatusWithSize(OkStatus(), chars_written);
}

}  // namespace pw::sys_io
