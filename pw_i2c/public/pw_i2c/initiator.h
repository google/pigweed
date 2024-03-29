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

#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_i2c/address.h"
#include "pw_status/status.h"

namespace pw::i2c {

/// @brief The common, base driver interface for initiating thread-safe
/// transactions with devices on an I2C bus. Other documentation may call this
/// style of interface an I2C "master", <!-- inclusive-language: disable -->
/// "central", or "controller".
///
/// `pw::i2c::Initiator` isn't required to support 10-bit addressing. If only
/// 7-bit addressing is supported, `pw::i2c::Initiator` fails a runtime
/// assertion when given an address that is out of 7-bit address range.
///
/// The implementer of this pure virtual interface is responsible for ensuring
/// thread safety and enabling functionality such as initialization,
/// configuration, enabling and disabling, unsticking SDA, and detecting device
/// address registration collisions.
///
/// @note `pw::i2c::Initiator` uses internal synchronization, so it's safe to
/// initiate transactions from multiple threads. However, a combined write and
/// read transaction may not be atomic when there are multiple initiators on
/// the bus. Furthermore, devices may require specific sequences of
/// transactions, and application logic must provide the synchronization to
/// execute these sequences correctly.
class Initiator {
 public:
  virtual ~Initiator() = default;

  /// Writes bytes to an I2C device and then reads bytes from that same
  /// device as either one atomic I2C transaction or two independent I2C
  /// transactions.
  ///
  /// If the I2C bus is a multi-initiator then the implementer of this
  /// class **must** ensure it's a single-atomic transaction.
  ///
  /// The signal on the bus for the atomic transaction should look like this:
  ///
  /// @code
  ///   START + I2C_ADDRESS + WRITE(0) + TX_BUFFER_BYTES +
  ///   START + I2C_ADDRESS + READ(1) + RX_BUFFER_BYTES + STOP
  /// @endcode
  ///
  /// The signal on the bus for the two independent transactions should look
  /// like this:
  ///
  /// @code
  ///   START + I2C_ADDRESS + WRITE(0) + TX_BUFFER_BYTES + STOP
  ///   START + I2C_ADDRESS + READ(1) + RX_BUFFER_BYTES + STOP
  /// @endcode
  ///
  /// @param[in] device_address The address of the I2C device.
  ///
  /// @param[in] tx_buffer The transmit buffer.
  ///
  /// @param[out] rx_buffer The receive buffer.
  ///
  /// @param[in] timeout The maximum duration to block waiting for both
  /// exclusive bus access and the completion of the I2C transaction or
  /// transactions.
  ///
  /// @pre The provided address must be supported by the initiator. I.e.
  /// don't use a 10-bit address if the initiator only supports 7-bit
  /// addresses. This method fails a runtime assertion if this precondition
  /// isn't met.
  ///
  /// @returns A `pw::Status` object with one of the following statuses:
  /// * @pw_status{OK} - The transaction or transactions succeeded.
  /// * @pw_status{INVALID_ARGUMENT} - The device address provided is bigger
  ///   than 10 bits.
  /// * @pw_status{DEADLINE_EXCEEDED} - Was unable to acquire exclusive
  ///   initiator access and complete the I2C transaction in time.
  /// * @pw_status{UNAVAILABLE} - A NACK condition occurred, meaning the
  ///   addressed device didn't respond or was unable to process the request.
  /// * @pw_status{FAILED_PRECONDITION} - The interface isn't initialized or
  ///   enabled.
  Status WriteReadFor(Address device_address,
                      ConstByteSpan tx_buffer,
                      ByteSpan rx_buffer,
                      chrono::SystemClock::duration timeout) {
    return DoWriteReadFor(device_address, tx_buffer, rx_buffer, timeout);
  }

  /// A variation of `pw::i2c::Initiator::WriteReadFor` that accepts explicit
  /// sizes for the amount of bytes to write to the device and read from the
  /// device.
  Status WriteReadFor(Address device_address,
                      const void* tx_buffer,
                      size_t tx_size_bytes,
                      void* rx_buffer,
                      size_t rx_size_bytes,
                      chrono::SystemClock::duration timeout) {
    return WriteReadFor(
        device_address,
        span(static_cast<const std::byte*>(tx_buffer), tx_size_bytes),
        span(static_cast<std::byte*>(rx_buffer), rx_size_bytes),
        timeout);
  }

  /// Write bytes to the I2C device.
  ///
  /// The signal on the bus should look like this:
  ///
  /// @code
  ///   START + I2C_ADDRESS + WRITE(0) + TX_BUFFER_BYTES + STOP
  /// @endcode
  ///
  /// @param[in] device_address The address of the I2C device.
  ///
  /// @param[in] tx_buffer The transmit buffer.
  ///
  /// @param[out] rx_buffer The receive buffer.
  ///
  /// @param[in] timeout The maximum duration to block waiting for both
  /// exclusive bus access and the completion of the I2C transaction.
  ///
  /// @pre The provided address must be supported by the initiator. I.e.
  /// don't use a 10-bit address if the initiator only supports 7-bit
  /// addresses. This method fails a runtime assertion if this precondition
  /// isn't met.
  ///
  /// @returns A `pw::Status` object with one of the following statuses:
  /// * @pw_status{OK} - The transaction succeeded.
  /// * @pw_status{INVALID_ARGUMENT} - The device address provided is bigger
  ///   than 10 bits.
  /// * @pw_status{DEADLINE_EXCEEDED} - Was unable to acquire exclusive
  ///   initiator access and complete the I2C transaction in time.
  /// * @pw_status{UNAVAILABLE} - A NACK condition occurred, meaning the
  ///   addressed device didn't respond or was unable to process the request.
  /// * @pw_status{FAILED_PRECONDITION} - The interface isn't initialized or
  ///   enabled.
  Status WriteFor(Address device_address,
                  ConstByteSpan tx_buffer,
                  chrono::SystemClock::duration timeout) {
    return WriteReadFor(device_address, tx_buffer, ByteSpan(), timeout);
  }
  /// A variation of `pw::i2c::Initiator::WriteFor` that accepts an explicit
  /// size for the amount of bytes to write to the device.
  Status WriteFor(Address device_address,
                  const void* tx_buffer,
                  size_t tx_size_bytes,
                  chrono::SystemClock::duration timeout) {
    return WriteFor(
        device_address,
        span(static_cast<const std::byte*>(tx_buffer), tx_size_bytes),
        timeout);
  }

  /// Reads bytes from an I2C device.
  ///
  /// The signal on the bus should look like this:
  ///
  /// @code
  ///   START + I2C_ADDRESS + READ(1) + RX_BUFFER_BYTES + STOP
  /// @endcode
  ///
  /// @param[in] device_address The address of the I2C device.
  ///
  /// @param[out] rx_buffer The receive buffer.
  ///
  /// @param[in] timeout The maximum duration to block waiting for both
  /// exclusive bus access and the completion of the I2C transaction.
  ///
  /// @pre The provided address must be supported by the initiator. I.e.
  /// don't use a 10-bit address if the initiator only supports 7-bit
  /// addresses. This method fails a runtime assertion if this precondition
  /// isn't met.
  ///
  /// @returns A `pw::Status` object with one of the following statuses:
  /// * @pw_status{OK} - The transaction succeeded.
  /// * @pw_status{INVALID_ARGUMENT} - The device address provided is bigger
  ///   than 10 bits.
  /// * @pw_status{DEADLINE_EXCEEDED} - Was unable to acquire exclusive
  ///   initiator access and complete the I2C transaction in time.
  /// * @pw_status{UNAVAILABLE} - A NACK condition occurred, meaning the
  ///   addressed device didn't respond or was unable to process the request.
  /// * @pw_status{FAILED_PRECONDITION} - The interface isn't initialized or
  ///   enabled.
  Status ReadFor(Address device_address,
                 ByteSpan rx_buffer,
                 chrono::SystemClock::duration timeout) {
    return WriteReadFor(device_address, ConstByteSpan(), rx_buffer, timeout);
  }
  /// A variation of `pw::i2c::Initiator::ReadFor` that accepts an explicit
  /// size for the amount of bytes to read from the device.
  Status ReadFor(Address device_address,
                 void* rx_buffer,
                 size_t rx_size_bytes,
                 chrono::SystemClock::duration timeout) {
    return ReadFor(device_address,
                   span(static_cast<std::byte*>(rx_buffer), rx_size_bytes),
                   timeout);
  }

  /// Probes the device for an I2C ACK after only writing the address.
  /// This is done by attempting to read a single byte from the specified
  /// device.
  ///
  /// @param[in] device_address The address of the I2C device.
  ///
  /// @param[in] timeout The maximum duration to block waiting for both
  /// exclusive bus access and the completion of the I2C transaction.
  ///
  /// @pre The provided address must be supported by the initiator. I.e.
  /// don't use a 10-bit address if the initiator only supports 7-bit
  /// addresses. This method fails a runtime assertion if this precondition
  /// isn't met.
  ///
  /// @returns A `pw::Status` object with one of the following statuses:
  /// * @pw_status{OK} - The transaction succeeded.
  /// * @pw_status{INVALID_ARGUMENT} - The device address provided is bigger
  ///   than 10 bits.
  /// * @pw_status{DEADLINE_EXCEEDED} - Was unable to acquire exclusive
  ///   initiator access and complete the I2C transaction in time.
  /// * @pw_status{UNAVAILABLE} - A NACK condition occurred, meaning the
  ///   addressed device didn't respond or was unable to process the request.
  /// * @pw_status{FAILED_PRECONDITION} - The interface isn't initialized or
  ///   enabled.
  Status ProbeDeviceFor(Address device_address,
                        chrono::SystemClock::duration timeout) {
    std::byte ignored_buffer[1] = {};  // Read a byte to probe.
    return WriteReadFor(
        device_address, ConstByteSpan(), ignored_buffer, timeout);
  }

 private:
  virtual Status DoWriteReadFor(Address device_address,
                                ConstByteSpan tx_buffer,
                                ByteSpan rx_buffer,
                                chrono::SystemClock::duration timeout) = 0;
};

}  // namespace pw::i2c
