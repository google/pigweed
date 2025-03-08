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
#include "pw_i2c_mcuxpresso/initiator.h"

#include <mutex>

#include "fsl_i2c.h"
#include "pw_chrono/system_clock.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::i2c {
namespace {

Status HalStatusToPwStatus(status_t status) {
  switch (status) {
    case kStatus_Success:
      return OkStatus();
    case kStatus_I2C_Nak:
    case kStatus_I2C_Addr_Nak:
      return Status::Unavailable();
    case kStatus_I2C_InvalidParameter:
      return Status::InvalidArgument();
    case kStatus_I2C_Timeout:
      return Status::DeadlineExceeded();
    default:
      return Status::Unknown();
  }
}
}  // namespace

// inclusive-language: disable
void McuxpressoInitiator::Enable() {
  std::lock_guard lock(mutex_);

  i2c_master_config_t master_config;
  I2C_MasterGetDefaultConfig(&master_config);
  master_config.baudRate_Bps = config_.baud_rate_bps;
  I2C_MasterInit(base_, &master_config, CLOCK_GetFreq(config_.clock_name));

  // Create the handle for the non-blocking transfer and register callback.
  I2C_MasterTransferCreateHandle(
      base_, &handle_, McuxpressoInitiator::TransferCompleteCallback, this);

  enabled_ = true;
}

void McuxpressoInitiator::Disable() {
  std::lock_guard lock(mutex_);
  I2C_MasterDeinit(base_);
  enabled_ = false;
}

McuxpressoInitiator::~McuxpressoInitiator() { Disable(); }

void McuxpressoInitiator::TransferCompleteCallback(I2C_Type*,
                                                   i2c_master_handle_t*,
                                                   status_t status,
                                                   void* initiator_ptr) {
  McuxpressoInitiator& initiator =
      *static_cast<McuxpressoInitiator*>(initiator_ptr);
  initiator.callback_isl_.lock();
  initiator.transfer_status_ = status;
  initiator.callback_isl_.unlock();
  initiator.callback_complete_notification_.release();
}

Status McuxpressoInitiator::InitiateNonBlockingTransferUntil(
    chrono::SystemClock::time_point deadline, i2c_master_transfer_t* transfer) {
  const status_t status =
      I2C_MasterTransferNonBlocking(base_, &handle_, transfer);
  if (status != kStatus_Success) {
    return HalStatusToPwStatus(status);
  }

  if (!callback_complete_notification_.try_acquire_until(deadline)) {
    I2C_MasterTransferAbort(base_, &handle_);
    return Status::DeadlineExceeded();
  }

  callback_isl_.lock();
  const status_t transfer_status = transfer_status_;
  callback_isl_.unlock();

  return HalStatusToPwStatus(transfer_status);
}

// Performs a sequence of non-blocking I2C reads and writes.
Status McuxpressoInitiator::DoTransferFor(
    span<const Message> messages, chrono::SystemClock::duration timeout) {
  chrono::SystemClock::time_point deadline =
      chrono::SystemClock::TimePointAfterAtLeast(timeout);

  std::lock_guard lock(mutex_);
  if (!enabled_) {
    return Status::FailedPrecondition();
  }

  pw::Status status = pw::OkStatus();
  for (unsigned int i = 0; i < messages.size(); ++i) {
    const Message& msg = messages[i];

    uint32_t i2c_flags = kI2C_TransferDefaultFlag;

    if (msg.IsWriteContinuation()) {
      i2c_flags |= kI2C_TransferNoStartFlag;
    } else if (i > 0) {
      // Use repeated start flag for all but the first message.
      i2c_flags |= kI2C_TransferRepeatedStartFlag;
    }

    // No stop flag prior to the final message.
    if (i < messages.size() - 1) {
      i2c_flags |= kI2C_TransferNoStopFlag;
    }
    i2c_master_transfer_t transfer{
        .flags = i2c_flags,
        .slaveAddress =
            msg.GetAddress().GetSevenBit(),  // Will CHECK if >7 bits.
        .direction = msg.IsRead() ? kI2C_Read : kI2C_Write,
        .subaddress = 0,
        .subaddressSize = 0,
        // Cast GetData() here because GetMutableData() is for Writes only.
        .data = const_cast<std::byte*>(msg.GetData().data()),
        .dataSize = msg.GetData().size()};
    status = InitiateNonBlockingTransferUntil(deadline, &transfer);
    if (!status.ok()) {
      PW_LOG_WARN("error on submessage %d of %d: status=%d",
                  i,
                  messages.size(),
                  status.code());
      break;
    }
  }

  return status;
}
// inclusive-language: enable

}  // namespace pw::i2c
