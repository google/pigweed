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

#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::uart {

/// Represents an abstract UART interface.
///
/// The `UartNonBlocking` interface provides a basic set of methods for
/// performing Non-Blocking UART communication. It defines the interface that
/// concrete UART implementations must adhere to.

class UartNonBlocking {
 public:
  virtual ~UartNonBlocking() = default;

  /// Initializes the UART module on the microcontroller, sets it into the
  /// default state as determined by the concrete UART implementation. This
  /// function should be a no-op if the UART module is in an enabled state.
  ///
  /// This may change the power state of the UART module, configure the
  /// interface parameters, enable the associated pins, setup the internal
  /// TX and RX buffers, etc...
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The UART module has been successfully initialized.
  ///
  ///    INTERNAL: Internal errors within the hardware abstraction layer.
  ///
  /// @endrst
  Status Enable() { return DoEnable(true); }

  /// Disables the UART module on the microcontroller. Disabling the UART
  /// shuts down communication and prevents the microcontroller from
  /// sending or receiving data through the UART port.
  ///
  /// This is usually done to save power. Interrupt handlers should also be
  /// disabled.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The UART module has been successfully initialized.
  ///
  ///    INTERNAL: Internal errors  within the hardware abstraction layer.
  ///
  /// @endrst
  Status Disable() { return DoEnable(false); }

  /// Configures the UART communication baud rate.
  ///
  /// This function sets the communication speed (baud rate) for the UART.
  /// Whether the baud rate can be changed while the UART is enabled depends on
  /// the specific implementation.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The UART has been successfully initialized.
  ///
  ///    FAILED_PRECONDITION: The device is enabled and does not support
  ///    changing settings on the fly.
  ///
  ///    INTERNAL: Internal errors  within the hardware abstraction layer.
  ///
  /// @endrst
  Status SetBaudRate(uint32_t baud_rate) { return DoSetBaudRate(baud_rate); }

  /// Reads exactly `rx_buffer.size()` bytes from the UART into the provided
  /// buffer.
  ///
  /// This function calls `callback` after the entirety of `rx_buffer` is filled
  /// with data. This may be called from interrupt context.
  /// The callback may be called in ISR context.
  /// It is safe to call `Read`/`Write` from the callback context.
  ///
  /// @param rx_buffer  The buffer to read data into.
  /// @param callback   A callback to invoke when the transaction is completed.
  ///                   @param status  OK: The operation was successful and the
  ///                                  entire buffer has been filled with data.
  ///                                  CANCELLED: The operation was cancelled
  ///                                  via CancelRead().
  ///                                  May return other implementation-specific
  ///                                  status codes.
  ///                   @param buffer  buffer.size() returns the number of
  ///                                  bytes successfully read into the buffer.
  ///                                  If `status`=OK, the buffer is identical
  ///                                  to rx_buffer
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successfully started.
  ///    UNAVAILABLE: Another Read() transaction is currently in progress.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  Status ReadExactly(
      ByteSpan rx_buffer,
      Function<void(Status status, ConstByteSpan buffer)>&& callback) {
    return DoRead(rx_buffer, rx_buffer.size(), std::move(callback));
  }

  /// Reads at least `min_bytes` and at most `rx_buffer.size()` bytes from the
  /// UART into the provided buffer.
  ///
  /// This function calls `callback` after the entirety of `rx_buffer` is filled
  /// with data. This may be called from interrupt context.
  /// The callback may be called in ISR context.
  /// It is safe to call `Read`/`Write` from the callback context.
  ///
  /// @param rx_buffer  The buffer to read data into.
  /// @param min_bytes  Minimum bytes to read.
  /// @param callback   A callback to invoke when the transaction is completed.
  ///                   @param status  OK: The operation was successful and the
  ///                                  buffer has been filled with at least
  ///                                  `min_bytes` with data.
  ///                                  CANCELLED: The operation was cancelled
  ///                                  via CancelRead().
  ///                                  May return other implementation-specific
  ///                                  status codes.
  ///                   @param buffer  buffer.size() returns the number of
  ///                                  bytes successfully read into the buffer.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successfully started.
  ///    UNAVAILABLE: Another Read() transaction is currently in progress.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  Status ReadAtLeast(
      ByteSpan rx_buffer,
      size_t min_bytes,
      Function<void(Status status, ConstByteSpan buffer)>&& callback) {
    return DoRead(rx_buffer, min_bytes, std::move(callback));
  }

  /// Cancel a current read in progress.
  ///
  /// This function will cancel a read in progress. The read's callback will
  /// be invoked with status=CANCELLED.
  ///
  /// @returns @rst
  ///
  ///    true: The operation was successfully canceled a transaction in progress
  ///          and the callback will be invoked with status=CANCELLED.
  ///    false: There were no transactions in progress and nothing was
  ///           cancelled. No callback will be invoked.
  ///
  /// @endrst
  bool CancelRead() { return DoCancelRead(); }

  /// Writes data from a provided buffer to the UART.
  ///
  /// This function calls `callback` after the entirety of `tx_buffer` is
  /// written to the UART. This may be called from interrupt context.
  /// The callback may be called in ISR context.
  /// It is safe to call `Read`/`Write` from the callback context.
  ///
  /// @param tx_buffer  The buffer to write to the UART.
  /// @param callback   A callback to invoke when the transaction is completed.
  ///                   @param status  status.size() returns the number of bytes
  ///                                  successfully written from `tx_buffer`.
  ///                                  OK: The operation was successful and the
  ///                                  entire buffer has been written.
  ///                                  CANCELLED: The operation was cancelled
  ///                                  via CancelWrite().
  ///                                  May return other implementation-specific
  ///                                  status codes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successfully started.
  ///    UNAVAILABLE: Another Write() transaction is currently in progress.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  Status Write(ConstByteSpan tx_buffer,
               Function<void(StatusWithSize status)>&& callback) {
    return DoWrite(tx_buffer, std::move(callback));
  }

  /// Cancel a current write in progress.
  ///
  /// This function will cancel a write in progress. The write's callback will
  /// be called with status=CANCELLED.
  ///
  /// @returns @rst
  ///
  ///    true: The operation was successfully canceled a transaction in progress
  ///          and the callback will be invoked with status=CANCELLED.
  ///    false: There were no transactions in progress and nothing was
  ///           cancelled. No callback will be invoked.
  ///
  /// @endrst
  bool CancelWrite() { return DoCancelWrite(); }

 private:
  virtual Status DoEnable(bool enable) = 0;
  virtual Status DoSetBaudRate(uint32_t baud_rate) = 0;

  /// Reads at least `min_bytes` and at most `rx_buffer.size()` bytes from the
  /// UART into the provided buffer.
  ///
  /// This virtual function attempts to read data into the provided byte buffer
  /// (`rx_buffer`).
  /// This virtual function will return immediately. `callback` will be invoked
  /// when the buffer has been filled with at least `min_bytes`, or an error
  /// occurs.
  /// Implementation Notes:
  /// * The callback may be called in ISR context.
  /// * It is safe to call `DoRead`/`DoWrite` from the callback context.
  /// * To prevent infinite recurssion the callback should never be called
  ///   synchronously from `DoRead`.
  /// * The callback must be moved and stored prior to its invocation.
  /// * Do not hold a lock when invoking the callback.
  ///
  /// @param rx_buffer  The buffer to read data into.
  /// @param min_bytes  Minimum bytes to read.
  /// @param callback   A callback to invoke when the transaction is completed.
  ///                   @param status  OK: The operation was successful and the
  ///                                  buffer has been filled with at least
  ///                                  `min_bytes` with data.
  ///                                  CANCELLED: The operation was cancelled
  ///                                  via CancelRead().
  ///                                  May return other implementation-specific
  ///                                  status codes.
  ///                   @param buffer  buffer.size() returns the number of
  ///                                  bytes successfully read into the buffer.
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful started.
  ///    UNAVAILABLE: Another Read() transaction is currently in progress.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  virtual Status DoRead(
      ByteSpan rx_buffer,
      size_t min_bytes,
      Function<void(Status status, ConstByteSpan buffer)>&& callback) = 0;

  /// Cancels a current read in progress.
  ///
  /// This virtual function will cancel a read in progress. The read's callback
  /// will be called with status=CANCELLED.
  ///
  /// @returns @rst
  ///
  ///    true: The operation was successfully canceled a transaction in progress
  ///          and the callback will be invoked with status=CANCELLED.
  ///    false: There were no transactions in progress and nothing was
  ///           cancelled. No callback will be invoked.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  virtual bool DoCancelRead() = 0;

  /// Writes data from a provided buffer to the UART.
  ///
  /// This virtual function attempts to write data from the provided byte
  /// buffer (`tx_buffer`).
  /// This virtual function will return immediately. `callback` will be invoked
  /// when either the buffer fully written, or an error occurs.
  /// Implementation Notes:
  /// * The callback may be called in ISR context.
  /// * It is safe to call `DoRead`/`DoWrite` from the callback context.
  /// * To prevent infinite recurssion the callback should never be called
  ///   synchronously from `DoRead`.
  /// * The callback must be moved and stored prior to its invocation.
  /// * Do not hold a lock when invoking the callback.
  ///
  /// @param tx_buffer  The buffer to write to the UART.
  /// @param callback   A callback to invoke when the transaction is completed.
  ///                   @param status  status.size() returns the number of bytes
  ///                                  successfully written from `tx_buffer`.
  ///                   @param status  OK: The operation was successful and the
  ///                                  entire buffer has been written.
  ///                                  CANCELLED: The operation was cancelled
  ///                                  via CancelWrite().
  ///                                  May return other implementation-specific
  ///                                  status codes.
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful started.
  ///    UNAVAILABLE: Another Write() transaction is currently in progress.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  virtual Status DoWrite(ConstByteSpan tx_buffer,
                         Function<void(StatusWithSize status)>&& callback) = 0;

  /// Cancel a current write in progress.
  ///
  /// This virtual function will cancel a write in progress. The write's
  /// callback will be invoked with status=CANCELLED.
  ///
  /// @returns @rst
  ///
  ///    true: The operation was successfully canceled a transaction in progress
  ///          and the callback will be invoked with status=CANCELLED.
  ///    false: There were no transactions in progress and nothing was
  ///           cancelled. No callback will be invoked.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  virtual bool DoCancelWrite() = 0;
};

}  // namespace pw::uart
