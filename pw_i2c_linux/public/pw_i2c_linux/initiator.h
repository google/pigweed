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

#include "pw_bytes/span.h"
#include "pw_chrono/system_clock.h"
#include "pw_i2c/address.h"
#include "pw_i2c/initiator.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/timed_mutex.h"

namespace pw::i2c {

/// Initiator interface implementation using the Linux userspace i2c-dev driver.
///
/// Takes exclusive control of an I2C bus device (ex. "/dev/i2c-0"). The user is
/// responsible to open the device node prior to creating the initiator. The
/// file descriptor is closed when the initiator object is destroyed.
///
/// The bus device must support the full I2C functionality. Users of the class
/// are encouraged to use the `OpenI2cBus` helper to ensure the bus is valid.
///
/// Access to the bus is guarded by an internal mutex, so this initiator can be
/// safely used from multiple threads.
///
class LinuxInitiator final : public Initiator {
 public:
  /// Open an I2C bus and validate that full I2C functionality is supported.
  ///
  /// @param[in] bus_path Path to the I2C bus device node.
  ///
  /// @retval OK              The device node was opened successfully.
  /// @retval InvalidArgument Failed to open the device node or to validate I2C
  ///                         functionality.
  ///
  /// @return The open file descriptor on success.
  ///
  static Result<int> OpenI2cBus(const char* bus_path);

  /// Construct an instantiator using an open file descriptor.
  /// The file descriptor is closed during destruction.
  ///
  /// @param[in] fd Valid file descriptor for an I2C device node.
  ///
  LinuxInitiator(int fd);

  LinuxInitiator(const LinuxInitiator&) = delete;
  LinuxInitiator& operator=(const LinuxInitiator&) = delete;

  ~LinuxInitiator() override;

 private:
  /// Implement pw::i2c::Initiator with the following additional requriements:
  ///  - Asserts that `device_address` is a 7-bit address.
  ///  - At least one of `tx_buffer` or `rx_buffer` must be not empty.
  ///    Otherwise, returns InvalidArgument.
  ///
  /// @note
  /// The timeout is used both for getting an exclusive lock on the initiator
  /// and for getting exclusive use of a multi-controller bus. If the timeout
  /// is zero or negative, the transaction will only execute if there is no
  /// contention at either level.
  ///
  Status DoWriteReadFor(Address device_address,
                        ConstByteSpan tx_buffer,
                        ByteSpan rx_buffer,
                        chrono::SystemClock::duration timeout) override
      PW_LOCKS_EXCLUDED(mutex_);

  Status DoWriteReadForLocked(uint8_t address,
                              ConstByteSpan tx_buffer,
                              ByteSpan rx_buffer,
                              chrono::SystemClock::duration timeout)
      PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  /// The file descriptor for the i2c-dev device representing this bus.
  const int fd_;

  /// This mutex is used to synchronize access across multiple retries.
  sync::TimedMutex mutex_;
};

}  // namespace pw::i2c
