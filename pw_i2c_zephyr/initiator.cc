// Copyright 2025 The Pigweed Authors
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
#include "pw_i2c_zephyr/initiator.h"

#include <zephyr/drivers/i2c.h>

#include <mutex>

namespace pw::i2c {

Status ZephyrInitiator::DoWriteReadFor(Address device_address,
                                       ConstByteSpan tx_buffer,
                                       ByteSpan rx_buffer,
                                       chrono::SystemClock::duration timeout) {
  if (timeout <= chrono::SystemClock::duration::zero()) {
    return Status::DeadlineExceeded();
  }

  // Zephyr doesn't have a timeout argument to the i2c API so as long as the
  // timeout isn't in the past, just run the command.

  if (!device_is_ready(dev_)) {
    return Status::FailedPrecondition();
  }

  const uint8_t address = device_address.GetSevenBit();
  std::lock_guard lock(mutex_);

  int rc = i2c_write_read(
      dev_,
      address,
      /*write_buf=*/reinterpret_cast<const uint8_t*>(tx_buffer.data()),
      /*num_write=*/tx_buffer.size(),
      /*read_buf=*/reinterpret_cast<uint8_t*>(rx_buffer.data()),
      /*num_read=*/rx_buffer.size());

  return rc == 0 ? pw::OkStatus() : pw::Status::Unavailable();
};

}  // namespace pw::i2c
