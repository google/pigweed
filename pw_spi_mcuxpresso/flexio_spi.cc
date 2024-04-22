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

#include "pw_spi_mcuxpresso/flexio_spi.h"

#include <cinttypes>
#include <mutex>

#include "fsl_flexio_spi.h"
#include "fsl_gpio.h"
#include "pw_chrono/system_clock.h"
#include "pw_log/log.h"
#include "pw_spi/initiator.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::spi {
namespace {

using namespace ::std::literals::chrono_literals;
constexpr auto kMaxWait = chrono::SystemClock::for_at_least(1000ms);

Status ToPwStatus(int32_t status) {
  switch (status) {
    // Intentional fall-through
    case kStatus_Success:
    case kStatus_FLEXIO_SPI_Idle:
      return OkStatus();
    case kStatus_ReadOnly:
      return Status::PermissionDenied();
    case kStatus_OutOfRange:
      return Status::OutOfRange();
    case kStatus_InvalidArgument:
      return Status::InvalidArgument();
    case kStatus_Timeout:
      return Status::DeadlineExceeded();
    case kStatus_NoTransferInProgress:
      return Status::FailedPrecondition();
    // Intentional fall-through
    case kStatus_Fail:
    default:
      PW_LOG_ERROR("Mcuxpresso FlexIO_SPI unknown error code: %" PRId32,
                   status);
      return Status::Unknown();
  }
}

}  // namespace

// inclusive-language: disable

McuxpressoFlexIoInitiator::~McuxpressoFlexIoInitiator() {
  if (is_initialized()) {
    FLEXIO_SPI_MasterDeinit(&flexio_spi_config_);
  }
}

void McuxpressoFlexIoInitiator::ConfigureClock(
    flexio_spi_master_config_t* masterConfig, ClockPolarity clockPolarity) {
  flexio_timer_config_t timerConfig = {};
  uint16_t timerDiv = 0;
  uint16_t timerCmp = 0;

  // Rather than modify the flexio_spi driver code to support negative clock
  // polarity, we duplicate the clock setup here to add support for inverting
  // the output for SPI mode CPOL=1.
  timerConfig.triggerSelect =
      FLEXIO_TIMER_TRIGGER_SEL_SHIFTnSTAT(flexio_spi_config_.shifterIndex[0]);
  timerConfig.triggerPolarity = kFLEXIO_TimerTriggerPolarityActiveLow;
  timerConfig.triggerSource = kFLEXIO_TimerTriggerSourceInternal;
  timerConfig.pinConfig = kFLEXIO_PinConfigOutput;
  timerConfig.pinSelect = flexio_spi_config_.SCKPinIndex;
  if (clockPolarity == ClockPolarity::kActiveLow) {
    timerConfig.pinPolarity = kFLEXIO_PinActiveLow;
  } else {
    timerConfig.pinPolarity = kFLEXIO_PinActiveHigh;
  }

  timerConfig.timerMode = kFLEXIO_TimerModeDual8BitBaudBit;
  timerConfig.timerOutput = kFLEXIO_TimerOutputZeroNotAffectedByReset;
  timerConfig.timerDecrement = kFLEXIO_TimerDecSrcOnFlexIOClockShiftTimerOutput;
  timerConfig.timerReset = kFLEXIO_TimerResetNever;
  timerConfig.timerDisable = kFLEXIO_TimerDisableOnTimerCompare;
  timerConfig.timerEnable = kFLEXIO_TimerEnableOnTriggerHigh;
  timerConfig.timerStop = kFLEXIO_TimerStopBitEnableOnTimerDisable;
  timerConfig.timerStart = kFLEXIO_TimerStartBitEnabled;

  timerDiv = static_cast<uint16_t>(src_clock_hz_ / masterConfig->baudRate_Bps);
  timerDiv = timerDiv / 2U - 1U;

  timerCmp = (static_cast<uint16_t>(masterConfig->dataMode) * 2U - 1U) << 8U;
  timerCmp |= timerDiv;

  timerConfig.timerCompare = timerCmp;

  FLEXIO_SetTimerConfig(flexio_spi_config_.flexioBase,
                        flexio_spi_config_.timerIndex[0],
                        &timerConfig);
}

void McuxpressoFlexIoInitiator::SpiCallback(FLEXIO_SPI_Type*,
                                            flexio_spi_master_handle_t*,
                                            status_t status,
                                            void* context) {
  auto* driver = static_cast<McuxpressoFlexIoInitiator*>(context);
  driver->last_transfer_status_ = ToPwStatus(status);
  driver->transfer_semaphore_.release();
}

Status McuxpressoFlexIoInitiator::Configure(const Config& config) {
  if (current_config_ && config == *current_config_) {
    return OkStatus();
  }

  flexio_spi_master_config_t master_config = {};
  FLEXIO_SPI_MasterGetDefaultConfig(&master_config);

  RESET_ClearPeripheralReset(kFLEXIO_RST_SHIFT_RSTn);

  if (config.phase == ClockPhase::kRisingEdge) {
    master_config.phase = kFLEXIO_SPI_ClockPhaseFirstEdge;
  } else {
    master_config.phase = kFLEXIO_SPI_ClockPhaseSecondEdge;
  }

  master_config.enableMaster = true;
  master_config.baudRate_Bps = baud_rate_bps_;

  switch (config.bits_per_word()) {
    case 8:
      master_config.dataMode = kFLEXIO_SPI_8BitMode;
      if (config.bit_order == BitOrder::kMsbFirst) {
        transfer_flags_ = kFLEXIO_SPI_8bitMsb;
      } else {
        transfer_flags_ = kFLEXIO_SPI_8bitLsb;
      }
      break;
    case 16:
      master_config.dataMode = kFLEXIO_SPI_16BitMode;
      if (config.bit_order == BitOrder::kMsbFirst) {
        transfer_flags_ = kFLEXIO_SPI_16bitMsb;
      } else {
        transfer_flags_ = kFLEXIO_SPI_16bitLsb;
      }
      break;
    default:
      return Status::InvalidArgument();
  }

  std::lock_guard lock(mutex_);
  FLEXIO_SPI_MasterInit(&flexio_spi_config_, &master_config, src_clock_hz_);
  ConfigureClock(&master_config, config.polarity);

  const auto status = ToPwStatus(FLEXIO_SPI_MasterTransferCreateHandle(
      &flexio_spi_config_,
      &driver_handle_,
      McuxpressoFlexIoInitiator::SpiCallback,
      this));

  if (status == OkStatus()) {
    current_config_.emplace(config);
  }
  return status;
}

Status McuxpressoFlexIoInitiator::WriteRead(ConstByteSpan write_buffer,
                                            ByteSpan read_buffer) {
  flexio_spi_transfer_t transfer = {};
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
    // Take smallest as size of transaction
    transfer.dataSize = write_buffer.size() < read_buffer.size()
                            ? write_buffer.size()
                            : read_buffer.size();
  }
  transfer.flags = transfer_flags_;

  std::lock_guard lock(mutex_);
  if (!current_config_) {
    PW_LOG_ERROR("Mcuxpresso FlexIO_SPI must be configured before use.");
    return Status::FailedPrecondition();
  }
  if (blocking_) {
    return ToPwStatus(
        FLEXIO_SPI_MasterTransferBlocking(&flexio_spi_config_, &transfer));
  }

  PW_TRY(ToPwStatus(FLEXIO_SPI_MasterTransferNonBlocking(
      &flexio_spi_config_, &driver_handle_, &transfer)));

  if (!transfer_semaphore_.try_acquire_for(kMaxWait)) {
    return Status::DeadlineExceeded();
  }
  return last_transfer_status_;
}

Status McuxpressoFlexIoChipSelector::SetActive(bool active) {
  return pin_.SetState(active ? digital_io::State::kInactive
                              : digital_io::State::kActive);
}
// inclusive-language: enable

}  // namespace pw::spi
