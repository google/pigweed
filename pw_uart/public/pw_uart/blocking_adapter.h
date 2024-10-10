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

#include "pw_sync/timed_thread_notification.h"
#include "pw_uart/uart.h"
#include "pw_uart/uart_non_blocking.h"

namespace pw::uart {

/// UartBlockingAdapter provides the blocking Uart interface on top of a
/// UartNonBlocking device.
class UartBlockingAdapter final : public Uart {
 public:
  /// Constructs a UartBlockingAdapter for a UartNonBlocking device.
  UartBlockingAdapter(UartNonBlocking& uart)
      : uart_(uart), rx_("rx"), tx_("tx") {}
  ~UartBlockingAdapter() override;

 private:
  UartNonBlocking& uart_;

  class Transfer {
   public:
    Transfer(const char* what) : what_(what) {}

    void Start() { pending_ = true; }
    void Complete(StatusWithSize result);
    void Complete(Status status) { Complete(StatusWithSize(status, 0)); }
    [[nodiscard]] bool WaitForCompletion(
        std::optional<chrono::SystemClock::duration> timeout);
    void WaitForCompletion();
    StatusWithSize HandleTimeout(bool cancel_result);

    bool pending() const { return pending_; }
    StatusWithSize result() const { return result_; }

   private:
    const char* what_ = nullptr;
    sync::TimedThreadNotification complete_;
    StatusWithSize result_;
    bool pending_ = false;
  };

  Transfer rx_;
  Transfer tx_;

  // UartBase impl.
  Status DoEnable(bool enable) override {
    return enable ? uart_.Enable() : uart_.Disable();
  }

  Status DoSetBaudRate(uint32_t baud_rate) override {
    return uart_.SetBaudRate(baud_rate);
  }

  Status DoSetFlowControl(bool enabled) override {
    return uart_.SetFlowControl(enabled);
  }

  size_t DoConservativeReadAvailable() override {
    return uart_.ConservativeReadAvailable();
  }

  Status DoClearPendingReceiveBytes() override {
    return uart_.ClearPendingReceiveBytes();
  }

  // Uart impl.
  StatusWithSize DoTryReadFor(
      ByteSpan rx_buffer,
      size_t min_bytes,
      std::optional<chrono::SystemClock::duration> timeout) override;

  StatusWithSize DoTryWriteFor(
      ConstByteSpan tx_buffer,
      std::optional<chrono::SystemClock::duration> timeout) override;

  Status DoFlushOutput() override;
};

}  // namespace pw::uart
