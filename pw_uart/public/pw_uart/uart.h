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

#include <cstddef>
#include <cstdint>
#include <optional>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::uart {

/// Represents an abstract UART interface.
///
/// The `Uart` interface provides a basic set of methods for performing UART
/// communication. It defines the interface that concrete UART implementations
/// must adhere to.

/// @defgroup pw_uart
/// @{
class Uart {
 public:
  virtual ~Uart() = default;

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
  ///    OK: The UART module has been succesfully initialized.
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
  ///    OK: The UART module has been succesfully initialized.
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
  ///    OK: The UART has been succesfully initialized.
  ///
  ///    FAILED_PRECONDITION: The device is enabled and does not support
  ///    changing settings on the fly.
  ///
  ///    INTERNAL: Internal errors  within the hardware abstraction layer.
  ///
  /// @endrst
  Status SetBaudRate(uint32_t baud_rate) { return DoSetBaudRate(baud_rate); }

  /// Reads data from the UART into a provided buffer.
  ///
  /// This function blocks until the entirety of `rx_buffer` is filled with
  /// data.
  ///
  /// @param rx_buffer  The buffer to read data into.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  Status Read(ByteSpan rx_buffer) {
    return DoTryReadFor(rx_buffer, std::nullopt).status();
  }

  /// Reads data from the UART into a provided buffer.
  ///
  /// This function blocks until either the entire buffer has been filled with
  /// data or the specified timeout has elapsed, whichever occurs first.
  ///
  /// @param rx_buffer  The buffer to read data into.
  /// @param timeout    The maximum time to wait for data to be read. If zero,
  ///                   the function will immediately return with at least one
  ///                   hardware read operation attempt.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful and the entire buffer has been filled
  ///    with data.
  ///
  ///    DEADLINE_EXCEEDED: The operation timed out before the entire buffer
  ///    could be filled.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  StatusWithSize TryReadFor(ByteSpan rx_buffer,
                            chrono::SystemClock::duration timeout) {
    return DoTryReadFor(rx_buffer, timeout);
  }

  /// Writes data from the provided buffer to the UART. The function blocks
  /// until the entire buffer has been written.
  ///
  /// @param tx_buffer - The buffer to write data from.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  Status Write(ConstByteSpan tx_buffer) {
    return DoTryWriteFor(tx_buffer, std::nullopt).status();
  }

  /// Writes data from the provided buffer to the UART. The function blocks
  /// until either the entire buffer has been written or the specified timeout
  /// has elapsed, whichever occurs first.
  ///
  /// @param tx_buffer  The buffer to write data from.
  /// @param timeout    The maximum time to wait for data to be written.
  ///                   If zero, the function will immediately return with at
  ///                   least one hardware write operation attempt.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful and the entire buffer has been
  ///    written.
  ///
  ///    DEADLINE_EXCEEDED: The operation timed out before the entire buffer
  ///    could be written.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  StatusWithSize TryWriteFor(ConstByteSpan tx_buffer,
                             chrono::SystemClock::duration timeout) {
    return DoTryWriteFor(tx_buffer, timeout);
  }

  /// Returns the number of bytes currently available for reading.
  ///
  /// This function checks the receive buffer to determine how many bytes of
  /// data are ready to be read.
  ///
  /// @returns The number of bytes available for reading. When no data is
  /// available or in case of an error this function returns 0.
  size_t ConservativeReadAvailable() { return DoConservativeReadAvailable(); }

  /// Blocks until all queued data in the UART  has been transmitted and the
  /// FIFO is empty.
  ///
  /// This function ensures that all data enqueued before calling this function
  /// has been transmitted. Any data enqueued after calling this function will
  /// be transmitted immediately.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  Status FlushOutput() { return DoFlushOutput(); }

  /// Empties the UART's receive buffer and discards any unread data.
  ///
  /// This function removes all data from the receive buffer, resetting the
  /// buffer to an empty state. This is useful for situations where you want to
  /// disregard any previously received data and resynchronize.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  Status ClearPendingReceiveBytes() { return DoClearPendingReceiveBytes(); }

 private:
  virtual Status DoEnable(bool enable) = 0;
  virtual Status DoSetBaudRate(uint32_t baud_rate) = 0;

  /// Reads data from the UART into a provided buffer with an optional timeout
  /// provided.
  ///
  /// This virtual function attempts to read data into the provided byte buffer
  /// (`rx_buffer`). The operation will continue until either the buffer is
  /// full, an error occurs, or the optional timeout duration expires.
  ///
  /// @param rx_buffer  The buffer to read data into.
  /// @param timeout    An optional timeout duration. If specified, the function
  ///                   will block for no longer than this duration. If zero,
  ///                   the function will immediately return with at least one
  ///                   hardware read operation attempt. If not specified, the
  ///                   function blocks untill the buffer is full.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful and the entire buffer has been
  ///    filled with data.
  ///
  ///    DEADLINE_EXCEEDED: The operation timed out before the entire buffer
  ///    could be filled.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  virtual StatusWithSize DoTryReadFor(
      ByteSpan rx_buffer,
      std::optional<chrono::SystemClock::duration> timeout) = 0;

  /// @brief Writes data from a provided buffer to the UART with an optional
  /// timeout.
  ///
  /// This virtual function attempts to write data from the provided byte buffer
  /// (`tx_buffer`) to the UART. The operation will continue until either the
  /// buffer is empty, an error occurs, or the optional timeout duration
  /// expires.
  ///
  /// @param tx_buffer  The buffer containing data to be written.
  /// @param timeout    An optional timeout duration. If specified, the function
  ///                   will block for no longer than this duration. If zero,
  ///                   the function will immediately return after at least one
  ///                   hardware write operation attempt. If not specified, the
  ///                   function blocks until the buffer is empty.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The operation was successful and the entire buffer has been
  ///    written.
  ///
  ///    DEADLINE_EXCEEDED: The operation timed out before the entire buffer
  ///    could be written.
  ///
  /// May return other implementation-specific status codes.
  ///
  /// @endrst
  virtual StatusWithSize DoTryWriteFor(
      ConstByteSpan tx_buffer,
      std::optional<chrono::SystemClock::duration> timeout) = 0;
  virtual size_t DoConservativeReadAvailable() = 0;
  virtual Status DoFlushOutput() = 0;
  virtual Status DoClearPendingReceiveBytes() = 0;
};

/// @}

}  // namespace pw::uart
