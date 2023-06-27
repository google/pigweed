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

#include "public/pw_spi_mcuxpresso/spi.h"

#include <cinttypes>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_log/log.h"
#include "pw_spi_mcuxpresso/spi.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::spi {
namespace {

using namespace ::std::literals::chrono_literals;
constexpr auto kMaxWait = pw::chrono::SystemClock::for_at_least(1000ms);

pw::Status ToPwStatus(int32_t status) {
  switch (status) {
    // Intentional fall-through
    case kStatus_Success:
    case kStatus_SPI_Idle:
      return pw::OkStatus();
    case kStatus_ReadOnly:
      return pw::Status::PermissionDenied();
    case kStatus_OutOfRange:
      return pw::Status::OutOfRange();
    case kStatus_InvalidArgument:
      return pw::Status::InvalidArgument();
    case kStatus_Timeout:
      return pw::Status::DeadlineExceeded();
    case kStatus_NoTransferInProgress:
      return pw::Status::FailedPrecondition();
    // Intentional fall-through
    case kStatus_Fail:
    default:
      PW_LOG_ERROR("Mcuxpresso SPI unknown error code: %d",
                   static_cast<int>(status));
      return pw::Status::Unknown();
  }
}

}  // namespace

McuxpressoInitiator::~McuxpressoInitiator() {
  if (is_initialized()) {
    SPI_Deinit(register_map_);
  }
}

// inclusive-language: disable
void McuxpressoInitiator::SpiCallback(SPI_Type*,
                                      spi_master_handle_t*,
                                      status_t status,
                                      void* context) {
  auto* driver = static_cast<McuxpressoInitiator*>(context);
  driver->last_transfer_status_ = ToPwStatus(status);
  driver->transfer_semaphore_.release();
}

Status McuxpressoInitiator::Configure(const Config& config) {
  std::lock_guard lock(mutex_);
  if (current_config_ && config == *current_config_) {
    return OkStatus();
  }
  return DoConfigure(config, lock);
}

Status McuxpressoInitiator::DoConfigure(const Config& config,
                                        const std::lock_guard<sync::Mutex>&) {
  spi_master_config_t master_config = {};
  SPI_MasterGetDefaultConfig(&master_config);

  if (config.polarity == ClockPolarity::kActiveLow) {
    master_config.polarity = kSPI_ClockPolarityActiveLow;
  } else {
    master_config.polarity = kSPI_ClockPolarityActiveHigh;
  }
  if (config.phase == ClockPhase::kRisingEdge) {
    master_config.phase = kSPI_ClockPhaseFirstEdge;
  } else {
    master_config.phase = kSPI_ClockPhaseSecondEdge;
  }

  if (config.bit_order == BitOrder::kMsbFirst) {
    master_config.direction = kSPI_MsbFirst;
  } else {
    master_config.direction = kSPI_LsbFirst;
  }

  master_config.enableMaster = true;
  master_config.baudRate_Bps = baud_rate_bps_;

  master_config.sselNum = static_cast<spi_ssel_t>(pin_);
  master_config.sselPol = static_cast<spi_spol_t>(kSPI_SpolActiveAllLow);

  // Data width enum value is 1 value below bits_per_word. i.e. 0 = 1;
  constexpr uint8_t kMinBitsPerWord = 4;
  constexpr uint8_t kMaxBitsPerWord = 16;
  PW_CHECK(config.bits_per_word() >= kMinBitsPerWord &&
           config.bits_per_word() <= kMaxBitsPerWord);
  master_config.dataWidth =
      static_cast<_spi_data_width>(config.bits_per_word() - 1);

  SPI_MasterInit(register_map_, &master_config, max_speed_hz_);
  const auto status = ToPwStatus(SPI_MasterTransferCreateHandle(
      register_map_, &driver_handle_, McuxpressoInitiator::SpiCallback, this));

  if (status == OkStatus()) {
    current_config_.emplace(config);
  }
  return status;
}

Status McuxpressoInitiator::WriteRead(ConstByteSpan write_buffer,
                                      ByteSpan read_buffer) {
  spi_transfer_t transfer = {};

  transfer.txData =
      reinterpret_cast<uint8_t*>(const_cast<std::byte*>(write_buffer.data()));
  transfer.rxData = reinterpret_cast<uint8_t*>(read_buffer.data());
  if (write_buffer.data() == nullptr && read_buffer.data() != nullptr) {
    // Read only transaction
    transfer.dataSize = read_buffer.size();
  } else if (read_buffer.data() == nullptr && write_buffer.data() != nullptr) {
    // Write only transaction
    transfer.dataSize = write_buffer.size();
  } else {
    // Take the smallest as the size of transaction
    transfer.dataSize = write_buffer.size() < read_buffer.size()
                            ? write_buffer.size()
                            : read_buffer.size();
  }
  transfer.configFlags = kSPI_FrameAssert;

  std::lock_guard lock(mutex_);
  if (!current_config_) {
    PW_LOG_ERROR("Mcuxpresso SPI must be configured before use.");
    return Status::FailedPrecondition();
  }
  if (blocking_) {
    return ToPwStatus(SPI_MasterTransferBlocking(register_map_, &transfer));
  }

  PW_TRY(ToPwStatus(SPI_MasterTransferNonBlocking(
      register_map_, &driver_handle_, &transfer)));

  if (!transfer_semaphore_.try_acquire_for(kMaxWait)) {
    return Status::DeadlineExceeded();
  }
  return last_transfer_status_;
}
// inclusive-language: enable

Status McuxpressoInitiator::SetChipSelect(uint32_t pin) {
  std::lock_guard lock(mutex_);
  if (pin == pin_) {
    return OkStatus();
  }
  pin_ = pin;
  // As configuration has not been called, must be called prior to use, and must
  // itself set chipselect, set will be delayed until configuration.
  if (!current_config_) {
    return OkStatus();
  }
  return DoConfigure(*current_config_, lock);
}

}  // namespace pw::spi
