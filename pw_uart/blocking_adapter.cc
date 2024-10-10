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

#include "pw_uart/blocking_adapter.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"

namespace pw::uart {

UartBlockingAdapter::~UartBlockingAdapter() {
  // We can't safely allow this because the driver still has a reference.
  // The safest thing to do here is crash.
  // Few applications are likely to ever call this destructor anyway.
  PW_CHECK(!rx_.pending());
  PW_CHECK(!tx_.pending());
}

// Uart impl.
StatusWithSize UartBlockingAdapter::DoTryReadFor(
    ByteSpan rx_buffer,
    size_t min_bytes,
    std::optional<chrono::SystemClock::duration> timeout) {
  if (rx_.pending()) {
    PW_LOG_ERROR("RX transaction already started");
    return StatusWithSize::Unavailable();
  }

  // Start a new transfer.
  rx_.Start();
  auto status = uart_.ReadAtLeast(
      rx_buffer, min_bytes, [this](Status xfer_status, ConstByteSpan buffer) {
        rx_.Complete(StatusWithSize(xfer_status, buffer.size()));
      });
  if (!status.ok()) {
    return StatusWithSize(status, 0);
  }

  // Wait for completion.
  if (rx_.WaitForCompletion(timeout)) {
    return rx_.result();
  }

  // Handle timeout by trying to cancel.
  return rx_.HandleTimeout(uart_.CancelRead());
}

StatusWithSize UartBlockingAdapter::DoTryWriteFor(
    ConstByteSpan tx_buffer,
    std::optional<chrono::SystemClock::duration> timeout) {
  if (tx_.pending()) {
    PW_LOG_ERROR("TX transaction already started");
    return StatusWithSize::Unavailable();
  }

  // Start a new transfer.
  tx_.Start();
  auto status = uart_.Write(
      tx_buffer, [this](StatusWithSize result) { tx_.Complete(result); });
  if (!status.ok()) {
    return StatusWithSize(status, 0);
  }

  // Wait for completion.
  if (tx_.WaitForCompletion(timeout)) {
    return tx_.result();
  }

  // Handle timeout by trying to cancel.
  return tx_.HandleTimeout(uart_.CancelWrite());
}

Status UartBlockingAdapter::DoFlushOutput() {
  if (tx_.pending()) {
    PW_LOG_ERROR("Flush or write already started");
    return Status::Unavailable();
  }

  // Start a flush.
  tx_.Start();
  PW_TRY(uart_.FlushOutput([this](Status result) { tx_.Complete(result); }));

  // Wait for completion.
  tx_.WaitForCompletion();
  return tx_.result().status();
}

// UartBlockingAdapter::Transfer
void UartBlockingAdapter::Transfer::Complete(StatusWithSize result) {
  result_ = result;
  pending_ = false;
  complete_.release();
}

bool UartBlockingAdapter::Transfer::WaitForCompletion(
    std::optional<chrono::SystemClock::duration> timeout) {
  if (timeout) {
    return complete_.try_acquire_for(*timeout);
  }
  complete_.acquire();
  return true;
}

void UartBlockingAdapter::Transfer::WaitForCompletion() {
  // With no timeout, this waits forever and must return true.
  PW_CHECK(WaitForCompletion(std::nullopt));
}

StatusWithSize UartBlockingAdapter::Transfer::HandleTimeout(
    bool cancel_result) {
  if (cancel_result) {
    // Cancelled successfully.
    //
    // The callback should have been invoked with a CANCELLED status, and
    // released the notification. Acquire the notification to safely retrieve
    // result.size().
    if (complete_.try_acquire()) {
      return StatusWithSize::DeadlineExceeded(result().size());
    }

    // We couldn't acquire the notification. The driver must be broken.
    PW_LOG_WARN(
        "Failed to acquire %s notification after successful cancel. "
        "UART driver seems to be broken!",
        what_);
  } else {
    // Couldn't cancel.
    //
    // Because we definitely started a transfer, either:
    // 1. The transaction finished just after the timeout. The callback ran
    //    (or is running); the notification was released (or is about to be
    //    released).
    // 2. The transaction couldn't be cancelled (past some point of no return).
    //    The callback will run with a non-CANCELLED status; the semaphore will
    //    be released.
    //
    // We could wait again, but there's really no point: If the completion
    // didn't already happen in the user-provided timeout, it seems unlikely to
    // happen now.
    //
    // Bail.
    PW_LOG_WARN("Failed to cancel %s transfer after timeout.", what_);
  }

  // Note that pending() may still be set, so future requests will fail.
  return StatusWithSize::DeadlineExceeded(0);
}

}  // namespace pw::uart
