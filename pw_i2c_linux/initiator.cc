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
#define PW_LOG_MODULE_NAME "I2C"

#include "pw_i2c_linux/initiator.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cinttypes>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_i2c/address.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::i2c {
namespace {

using ::pw::chrono::SystemClock;

// Returns an appropriate status code for the given fault_code (i.e. `errno`).
// For unexpected fault codes, logs messages to aid in debugging.
// Reference: https://www.kernel.org/doc/html/latest/i2c/fault-codes.html
Status PwStatusAndLog(int i2c_errno, uint8_t device_address) {
  switch (i2c_errno) {
    case EAGAIN:
      // Lost arbitration on a multi-controller bus.
      // This is a normal condition on multi-controller busses.
      return Status::Aborted();
    case ENOENT:
    case ENODEV:
    case ENXIO:
    case EREMOTEIO:
      // Generally indicates the device is unavailable or faulty.
      // This is a normal condition when incorrect address is specified.
      //
      // Return Unavailable instead of NotFound as per the requirements of
      // pw::i2c::Initiator.
      PW_LOG_INFO("I2C device unavailable at address 0x%" PRIx8,
                  device_address);
      return Status::Unavailable();
    case ESHUTDOWN:
      // It's not really clear what would cause a bus to be "suspended".
      PW_LOG_WARN("I2C bus is suspended");
      return Status::FailedPrecondition();
    default:
      // All other errors are unexpected and don't have a well-defined code.
      PW_LOG_ERROR("I2C transaction failed for address 0x%" PRIx8 ": errno=%d",
                   device_address,
                   i2c_errno);
      return Status::Unknown();
  }
}

}  // namespace

// Open the file at the given path and validate that it is a valid bus device.
Result<int> LinuxInitiator::OpenI2cBus(const char* bus_path) {
  int fd = open(bus_path, O_RDWR);
  if (fd < 0) {
    PW_LOG_ERROR(
        "Unable to open I2C bus device [%s]: errno=%d", bus_path, errno);
    return Status::InvalidArgument();
  }

  // Verify that the bus supports full I2C functionality.
  unsigned long functionality = 0;
  if (ioctl(fd, I2C_FUNCS, &functionality) != 0) {
    PW_LOG_ERROR("Unable to read I2C functionality for bus [%s]: errno=%d",
                 bus_path,
                 errno);
    close(fd);
    return Status::InvalidArgument();
  }

  if ((functionality & I2C_FUNC_I2C) == 0) {
    PW_LOG_ERROR("I2C bus [%s] does not support full I2C functionality",
                 bus_path);
    close(fd);
    return Status::InvalidArgument();
  }
  return fd;
}

LinuxInitiator::LinuxInitiator(int fd) : fd_(fd) { PW_DCHECK(fd_ >= 0); }

LinuxInitiator::~LinuxInitiator() {
  PW_DCHECK(fd_ >= 0);
  close(fd_);
}

Status LinuxInitiator::DoWriteReadFor(Address device_address,
                                      ConstByteSpan tx_buffer,
                                      ByteSpan rx_buffer,
                                      SystemClock::duration timeout) {
  auto start_time = SystemClock::now();

  // Validate arguments.
  const auto address = device_address.GetSevenBit();
  if (tx_buffer.empty() && rx_buffer.empty()) {
    PW_LOG_ERROR("At least one of tx_buffer or rx_buffer must be not empty");
    return Status::InvalidArgument();
  }

  // Try to acquire access to the bus.
  if (!mutex_.try_lock_for(timeout)) {
    return Status::DeadlineExceeded();
  }
  std::lock_guard lock(mutex_, std::adopt_lock);
  const auto elapsed = SystemClock::now() - start_time;
  return DoWriteReadForLocked(address, tx_buffer, rx_buffer, timeout - elapsed);
}

// Perform an I2C write, read, or combined write+read transaction.
//
// Preconditions:
//  - `this->mutex_` is acquired
//  - `this->fd_` is open for read/write and supports full I2C functionality.
//  - `address` is a 7-bit device address
//  - At least one of `tx_buffer` or `rx_buffer` is not empty.
//
// The transaction will be retried if we can't get access to the bus, until
// the timeout is reached. There will be no retries if `timeout` is zero or
// negative.
Status LinuxInitiator::DoWriteReadForLocked(
    uint8_t address,
    ConstByteSpan tx_buffer,
    ByteSpan rx_buffer,
    chrono::SystemClock::duration timeout) {
  const auto start_time = SystemClock::now();

  // Prepare messages for either a read, write, or combined transaction.
  // Populate `ioctl_data` with either one or two `i2c_msg` operations.
  // Use the `messages` buffer to store the operations.
  i2c_rdwr_ioctl_data ioctl_data{};
  std::array<i2c_msg, 2> messages{};
  if (!tx_buffer.empty() && rx_buffer.empty()) {
    messages[0] = i2c_msg{
        .addr = address,
        .flags = 0,  // Read transaction
        .len = static_cast<uint16_t>(tx_buffer.size()),
        .buf = reinterpret_cast<uint8_t*>(
            const_cast<std::byte*>(tx_buffer.data())),  // NOLINT: read-only
    };
    ioctl_data = {
        .msgs = messages.data(),
        .nmsgs = 1,
    };
  } else if (!rx_buffer.empty() && tx_buffer.empty()) {
    messages[0] = i2c_msg{
        .addr = address,
        .flags = I2C_M_RD,
        .len = static_cast<uint16_t>(rx_buffer.size()),
        .buf = reinterpret_cast<uint8_t*>(rx_buffer.data()),
    };
    ioctl_data = {
        .msgs = messages.data(),
        .nmsgs = 1,
    };
  } else {
    // DoWriteReadFor already checks that at least one buffer has data.
    // This is just an internal consistency check.
    PW_DCHECK(!rx_buffer.empty() && !tx_buffer.empty());
    messages[0] = i2c_msg{
        .addr = address,
        .flags = 0,  // Read transaction
        .len = static_cast<uint16_t>(tx_buffer.size()),
        .buf = reinterpret_cast<uint8_t*>(
            const_cast<std::byte*>(tx_buffer.data())),  // NOLINT: read-only
    };
    messages[1] = i2c_msg{
        .addr = address,
        .flags = I2C_M_RD,
        .len = static_cast<uint16_t>(rx_buffer.size()),
        .buf = reinterpret_cast<uint8_t*>(rx_buffer.data()),
    };
    ioctl_data = {
        .msgs = messages.data(),
        .nmsgs = 2,
    };
  }
  PW_LOG_DEBUG("Attempting I2C transaction with %" PRIu32 " operations",
               ioctl_data.nmsgs);

  // Attempt the transaction. If we can't get exclusive access to the bus,
  // then keep trying until we run out of time.
  do {
    if (ioctl(fd_, I2C_RDWR, &ioctl_data) < 0) {
      Status status = PwStatusAndLog(errno, address);
      if (status == Status::Aborted()) {
        // Lost arbitration and need to try again.
        PW_LOG_DEBUG("Retrying I2C transaction");
        continue;
      }
      return status;
    }
    return OkStatus();
  } while (SystemClock::now() - start_time < timeout);

  // Attempt transaction one last time. This thread may have been suspended
  // after the last attempt, but before the timeout actually expired. The
  // timeout is meant to be a minimum time period.
  if (ioctl(fd_, I2C_RDWR, &ioctl_data) < 0) {
    Status status = PwStatusAndLog(errno, address);
    if (status == Status::Aborted()) {
      PW_LOG_INFO("Timeout waiting for I2C bus access");
      return Status::DeadlineExceeded();
    }
    return status;
  }
  return OkStatus();
}

}  // namespace pw::i2c
