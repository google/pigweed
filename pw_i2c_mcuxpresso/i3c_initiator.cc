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
#include "pw_i2c_mcuxpresso/i3c_initiator.h"

#include <mutex>

#include "lib/stdcompat/utility.h"
#include "pw_bytes/span.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

using cpp23::to_underlying;

namespace pw::i2c {
namespace {
pw::Status HalStatusToPwStatus(status_t status) {
  switch (status) {
    case kStatus_Success:
      return pw::OkStatus();
    case kStatus_I3C_Timeout:
      return pw::Status::DeadlineExceeded();
    case kStatus_I3C_Nak:
    case kStatus_I3C_Busy:
    case kStatus_I3C_IBIWon:
    case kStatus_I3C_WriteAbort:
      return pw::Status::Unavailable();
    case kStatus_I3C_HdrParityError:
    case kStatus_I3C_CrcError:
    case kStatus_I3C_MsgError:
      return pw::Status::DataLoss();
    default:
      return pw::Status::Unknown();
  }
}

constexpr uint32_t kI3cInitOpenDrainBaudRate = 2000000;
constexpr uint32_t kI3cInitPushPullBaudRate = 4000000;
constexpr bool kI3cInitEnableOpenDrainHigh = false;
constexpr uint8_t kBroadcastAddressRaw = 0x7E;
constexpr pw::i2c::Address kBroadcastAddress =
    pw::i2c::Address::SevenBit<kBroadcastAddressRaw>();
std::array kDisecBuffer = {std::byte{0x0b}};
}  // namespace

// inclusive-language: disable
void I3cMcuxpressoInitiator::Enable() {
  std::lock_guard lock(mutex_);
  if (enabled_) {
    return;
  }

  i3c_master_config_t masterConfig;
  I3C_MasterGetDefaultConfig(&masterConfig);

  masterConfig.baudRate_Hz.i2cBaud = config_.i2c_baud_rate;
  masterConfig.baudRate_Hz.i3cOpenDrainBaud = config_.i3c_open_drain_baud_rate;
  masterConfig.baudRate_Hz.i3cPushPullBaud = config_.i3c_push_pull_baud_rate;
  masterConfig.enableOpenDrainStop = config_.enable_open_drain_stop;
  masterConfig.enableOpenDrainHigh = config_.enable_open_drain_high;

  I3C_MasterInit(base_, &masterConfig, CLOCK_GetI3cClkFreq());
  enabled_ = true;
}

void I3cMcuxpressoInitiator::Disable() {
  std::lock_guard lock(mutex_);
  if (!enabled_) {
    return;
  }

  I3C_MasterDeinit(base_);
  enabled_ = false;
}

pw::Status I3cMcuxpressoInitiator::SetDynamicAddressList(
    pw::span<const uint8_t> dynamic_address_list) {
  if (i3c_dynamic_address_list_.has_value()) {
    PW_LOG_ERROR("i3c_dynamic_address_list_ can only be set once");
    return pw::Status::AlreadyExists();
  }

  pw::Vector<uint8_t, I3C_MAX_DEVCNT> address_list_temp;
  size_t dynamic_address_num = dynamic_address_list.size();
  if (dynamic_address_num > I3C_MAX_DEVCNT) {
    PW_LOG_WARN("Only the first %d dynamic addresses are accepted",
                I3C_MAX_DEVCNT);
    dynamic_address_num = I3C_MAX_DEVCNT;
  }
  address_list_temp.resize(dynamic_address_num);
  std::copy(dynamic_address_list.begin(),
            dynamic_address_list.begin() + dynamic_address_num,
            address_list_temp.begin());
  i3c_dynamic_address_list_.emplace(address_list_temp);

  return pw::OkStatus();
}

pw::Status I3cMcuxpressoInitiator::Initialize() {
  std::lock_guard lock(mutex_);
  i3c_master_config_t masterConfig;

  if (!i3c_dynamic_address_list_.has_value() ||
      i3c_dynamic_address_list_->empty()) {
    PW_LOG_ERROR("Cannot initialize the bus without dynamic address");
    return pw::Status::FailedPrecondition();
  }

  // Initialize I3C master with low I3C speed to match I3C timing requirement
  // (mipi_I3C-Basic_specification_v1-1-1 section 6.2 Table 86 I3C Open Drain
  // Timing Parameters)
  I3C_MasterGetDefaultConfig(&masterConfig);
  masterConfig.baudRate_Hz.i2cBaud = config_.i2c_baud_rate;
  masterConfig.baudRate_Hz.i3cOpenDrainBaud = kI3cInitOpenDrainBaudRate;
  masterConfig.baudRate_Hz.i3cPushPullBaud = kI3cInitPushPullBaudRate;
  masterConfig.enableOpenDrainStop = config_.enable_open_drain_stop;
  masterConfig.enableOpenDrainHigh = kI3cInitEnableOpenDrainHigh;
  I3C_MasterInit(base_, &masterConfig, CLOCK_GetI3cClkFreq());

  // Broadcast RSTDAA
  // TODO: b/312487906 - First broadcast CCC receives NANK randomly on random
  // devices.
  if (pw::Status status = DoTransferCcc(I3cCccAction::kWrite,
                                        I3cCcc::kRstdaaBroadcast,
                                        kBroadcastAddress,
                                        pw::ByteSpan());
      !(status.ok())) {
    if (status != pw::Status::Unavailable()) {
      return status;
    }
    PW_LOG_WARN("Failed to broadcast first CCC, trying again...");
    PW_TRY(DoTransferCcc(I3cCccAction::kWrite,
                         I3cCcc::kRstdaaBroadcast,
                         kBroadcastAddress,
                         pw::ByteSpan()));
  }
  // Broadcast DISEC 0x0b
  PW_TRY(DoTransferCcc(I3cCccAction::kWrite,
                       I3cCcc::kDisecBroadcast,
                       kBroadcastAddress,
                       kDisecBuffer));
  // DAA
  status_t hal_status = I3C_MasterProcessDAA(base_,
                                             i3c_dynamic_address_list_->data(),
                                             i3c_dynamic_address_list_->size());
  if (hal_status != kStatus_Success) {
    PW_LOG_ERROR("Failed to initialize the I3C bus...");
  }

  // Re-initialize I3C master with user provided speed.
  I3C_MasterGetDefaultConfig(&masterConfig);
  masterConfig.baudRate_Hz.i2cBaud = config_.i2c_baud_rate;
  masterConfig.baudRate_Hz.i3cOpenDrainBaud = config_.i3c_open_drain_baud_rate;
  masterConfig.baudRate_Hz.i3cPushPullBaud = config_.i3c_push_pull_baud_rate;
  masterConfig.enableOpenDrainStop = config_.enable_open_drain_stop;
  masterConfig.enableOpenDrainHigh = config_.enable_open_drain_high;
  I3C_MasterInit(base_, &masterConfig, CLOCK_GetI3cClkFreq());

  return HalStatusToPwStatus(hal_status);
}

pw::Status I3cMcuxpressoInitiator::DoTransferCcc(I3cCccAction rnw,
                                                 I3cCcc ccc_id,
                                                 pw::i2c::Address address,
                                                 pw::ByteSpan buffer) {
  status_t status;
  i3c_master_transfer_t transfer;

  if (!enabled_) {
    return pw::Status::FailedPrecondition();
  }

  if (!(to_underlying(ccc_id) & kCccDirectBit)) {  // broadcast
    transfer.flags = kI3C_TransferDefaultFlag;
    transfer.slaveAddress = kBroadcastAddressRaw;
    transfer.direction = kI3C_Write;
    transfer.subaddress = static_cast<uint32_t>(ccc_id);
    transfer.subaddressSize = 1;
    transfer.data = buffer.data();
    transfer.dataSize = buffer.size();
    transfer.busType = kI3C_TypeI3CSdr;
    status = I3C_MasterTransferBlocking(base_, &transfer);
  } else {  // direct
    transfer.flags = kI3C_TransferDefaultFlag;
    transfer.slaveAddress = kBroadcastAddressRaw;
    transfer.direction = kI3C_Write;
    transfer.subaddress = static_cast<uint32_t>(ccc_id);
    transfer.subaddressSize = 1;
    transfer.data = nullptr;
    transfer.dataSize = 0;
    transfer.busType = kI3C_TypeI3CSdr;
    status = I3C_MasterTransferBlocking(base_, &transfer);
    if (status != kStatus_Success) {
      return HalStatusToPwStatus(status);
    }

    transfer.flags = kI3C_TransferRepeatedStartFlag;
    transfer.slaveAddress = uint32_t{address.GetSevenBit()};
    transfer.direction = (rnw == I3cCccAction::kWrite) ? kI3C_Write : kI3C_Read;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = buffer.data();
    transfer.dataSize = buffer.size();
    transfer.busType = kI3C_TypeI3CSdr;
    status |= I3C_MasterTransferBlocking(base_, &transfer);
  }
  return HalStatusToPwStatus(status);
}

pw::Status I3cMcuxpressoInitiator::DoWriteReadFor(
    pw::i2c::Address address,
    pw::ConstByteSpan tx_buffer,
    pw::ByteSpan rx_buffer,
    pw::chrono::SystemClock::duration) {
  std::lock_guard lock(mutex_);
  status_t status;
  i3c_master_transfer_t transfer;

  if (!enabled_) {
    return pw::Status::FailedPrecondition();
  }

  if (std::find(i3c_dynamic_address_list_->begin(),
                i3c_dynamic_address_list_->end(),
                address.GetSevenBit()) == i3c_dynamic_address_list_->end()) {
    transfer.busType = kI3C_TypeI2C;
  } else {
    transfer.busType = kI3C_TypeI3CSdr;
  }

  if (!tx_buffer.empty() && rx_buffer.empty()) {  // write only
    transfer.flags = kI3C_TransferDefaultFlag;
    transfer.slaveAddress = address.GetSevenBit();
    transfer.direction = kI3C_Write;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = const_cast<std::byte*>(tx_buffer.data());
    transfer.dataSize = tx_buffer.size();
    transfer.ibiResponse = kI3C_IbiRespNack;
    status = I3C_MasterTransferBlocking(base_, &transfer);
  } else if (tx_buffer.empty() && !rx_buffer.empty()) {  // read only
    transfer.flags = kI3C_TransferDefaultFlag;
    transfer.slaveAddress = address.GetSevenBit();
    transfer.direction = kI3C_Read;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = rx_buffer.data();
    transfer.dataSize = rx_buffer.size();
    transfer.ibiResponse = kI3C_IbiRespNack;
    status = I3C_MasterTransferBlocking(base_, &transfer);
  } else if (!tx_buffer.empty() && !rx_buffer.empty()) {  // write and read
    transfer.flags = kI3C_TransferNoStopFlag;
    transfer.slaveAddress = address.GetSevenBit();
    transfer.direction = kI3C_Write;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = const_cast<std::byte*>(tx_buffer.data());
    transfer.dataSize = tx_buffer.size();
    transfer.ibiResponse = kI3C_IbiRespNack;
    status = I3C_MasterTransferBlocking(base_, &transfer);
    if (status != kStatus_Success) {
      return HalStatusToPwStatus(status);
    }

    transfer.flags = kI3C_TransferRepeatedStartFlag;
    transfer.slaveAddress = address.GetSevenBit();
    transfer.direction = kI3C_Read;
    transfer.subaddress = 0;
    transfer.subaddressSize = 0;
    transfer.data = rx_buffer.data();
    transfer.dataSize = rx_buffer.size();
    transfer.ibiResponse = kI3C_IbiRespNack;
    status = I3C_MasterTransferBlocking(base_, &transfer);
  } else {
    return pw::Status::InvalidArgument();
  }

  return HalStatusToPwStatus(status);
}
// inclusive-language: enable

}  // namespace pw::i2c
